/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OpticsSystem.h"
#include "../ShaderDefines.h"
#include "../terrain/TerrainNode.h"
#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    struct TerrainOpticsCache
    {
        SRS terrainSRS;
        glm::dmat4 lastOpticsWorld = glm::dmat4(1.0);
        glm::dvec3 focalPoint = glm::dvec3(0.0);
        double focalDistance = 1.0;
        std::uint64_t lastTerrainRevision = 0u;
        bool valid = false;
    };

    bool matricesEqual(const glm::dmat4& lhs, const glm::dmat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (lhs[column][row] != rhs[column][row])
                    return false;
        return true;
    }

    bool makeWorldMatrix(const Transform& transform, const SRS& worldSRS, glm::dmat4& output)
    {
        auto position = transform.position.transform(worldSRS);
        if (!position.valid())
            return false;

        output = transform.topocentric && worldSRS.isGeocentric() ?
            worldSRS.topocentricToWorldMatrix({ position.x, position.y, position.z }) :
            glm::translate(glm::dmat4(1.0), glm::dvec3(position.x, position.y, position.z));
        output *= transform.localMatrix;
        return true;
    }

    void updateClipDistances(
        const Optics& optics,
        bool autoComputeNearFar,
        const glm::dvec3& forward,
        const glm::dvec3& terrainNormal,
        OpticsViewDetail& detail)
    {
        detail.nearDistance = detail.focalDistance * optics.nearScale + optics.nearBias;
        detail.farDistance = detail.focalDistance * optics.farScale + optics.farBias;

        if (optics.projection != Optics::Projection::Perspective ||
            !autoComputeNearFar || !detail.focalPointValid)
            return;

        auto clamp01 = [](double value)
            { return std::clamp(value, 0.0, 1.0); };

        // alpha: center-ray angle from terrain normal (0=head-on, pi/2=grazing)
        double cosAlpha = clamp01(glm::abs(glm::dot(forward, terrainNormal)));
        cosAlpha = std::max(1e-6, cosAlpha);
        double alpha = acos(cosAlpha);

        // Frustum diagonal half-angle from center ray to a corner ray.
        double tanHalfY = tan(glm::radians(optics.fovY * 0.5));
        double tanHalfX = tanHalfY * optics.aspectRatio;
        double coneHalfAngle = atan(std::sqrt(tanHalfX * tanHalfX + tanHalfY * tanHalfY));

        // Eye-to-plane distance along terrain normal.
        double h = detail.focalDistance * cosAlpha;

        // Corner-ray incidence limits.
        double nearAngle = alpha - coneHalfAngle;
        double farAngle = alpha + coneHalfAngle;

        // Keep far angle away from 90 deg singularity.
        const double maxAngle = glm::radians(89.0);
        if (farAngle > maxAngle) farAngle = maxAngle;

        // Distance along ray to hit flat tangent plane.
        // If nearAngle < 0, center ray is the closest valid hit.
        double nearRaw =
            (nearAngle <= 0.0) ? detail.focalDistance : (h / std::max(cos(nearAngle), 1e-6));
        double farRaw = h / std::max(cos(farAngle), 1e-6);

        // Padding and hard floors for stable clipping.
        double nearPad = std::max(1.0, detail.focalDistance * 0.05);
        double farPad = std::max(1.0, detail.focalDistance * 0.01);
        double nearClip = std::max(1.0, nearRaw - nearPad);
        double farClip = std::max(nearClip + 1.0, farRaw + farPad);

        detail.nearDistance = nearClip * optics.nearScale + optics.nearBias;
        detail.farDistance = farClip * optics.farScale + optics.farBias;
    }
}

void OpticsSystemNode::on_construct_Optics(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<OpticsDetail>(e);
}

void OpticsSystemNode::on_destroy_Optics(entt::registry& r, entt::entity e)
{
    r.remove<OpticsDetail>(e);
    r.remove<TerrainOpticsCache>(e);
}

OpticsSystemNode::OpticsSystemNode(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<Optics>().connect<&OpticsSystemNode::on_construct_Optics>(*this);
            r.on_destroy<Optics>().connect<&OpticsSystemNode::on_destroy_Optics>(*this);

            r.view<Optics>().each([&](auto entity, auto&)
                {
                    (void)r.get_or_emplace<OpticsDetail>(entity);
                });
    });
}

void
OpticsSystemNode::updateTargetSubscription()
{
    auto currentTarget = target.ref_ptr();
    auto subscribedTarget = _subscribedTarget.ref_ptr();
    if (currentTarget == subscribedTarget)
        return;

    _terrainSubscriptions.clear();
    _subscribedTarget = currentTarget;
    _targetHasChangeNotifications = false;
    _terrainRevision.fetch_add(1u, std::memory_order_relaxed);

    if (auto terrain = currentTarget.cast<TerrainNode>())
    {
        _targetHasChangeNotifications = true;
        _terrainSubscriptions += terrain->onTileLoaded([this](const TileKey&)
        {
            // Tile callbacks can arrive while a provisional, low-LOD terrain hit
            // is cached. Invalidate it so the next update samples refined geometry.
            _terrainRevision.fetch_add(1u, std::memory_order_relaxed);
        });
    }
}

void
OpticsSystemNode::updateOptics(VSGContext vsgcontext)
{
    auto writer = _registry.write();
    auto& registry = writer.registry;
    auto terrainTarget = target.ref_ptr();
    auto terrainNode = terrainTarget.cast<TerrainNode>();
    const SRS terrainSRS = terrainNode ? terrainNode->renderingSRS : SRS{};

    registry.view<TerrainClamp>().each([&](auto entity, auto& clamp)
        {
            clamp.owner = entity;
        });

    const auto terrainRevision = _terrainRevision.load(std::memory_order_relaxed);
    const bool canCache = _targetHasChangeNotifications;

    auto update = [&](auto entity, auto& optics, auto& opticsDetails, auto& transformDetail)
    {
        bool autoComputeFocalDistance = optics.autoComputeFocalDistance;
        bool autoComputeNearFar = optics.autoComputeNearFar;
        if (auto* terrainClamp = registry.try_get<TerrainClamp>(entity))
        {
            autoComputeFocalDistance = terrainClamp->enabled;
            autoComputeNearFar = terrainClamp->enabled && terrainClamp->computeClipRange;
        }

        // A TerrainNode's intersection coordinates are expressed in its own
        // rendering SRS. Compute the physical lens-to-terrain result there,
        // independently of the SRS used by any particular render view, and
        // transform only the resulting focal point into each view.
        if (autoComputeFocalDistance && terrainNode && terrainSRS.valid())
        {
            auto& cache = registry.get_or_emplace<TerrainOpticsCache>(entity);
            glm::dmat4 entityWorld;
            if (!makeWorldMatrix(transformDetail.sync, terrainSRS, entityWorld))
            {
                cache.valid = false;
                for (ViewIDType viewID : vsgcontext->activeViewIDs)
                {
                    auto& detail = opticsDetails.views[viewID];
                    detail.focalDistance = optics.focalDistance;
                    detail.focalPointValid = false;
                    detail.autoComputeCacheValid = false;
                    updateClipDistances(
                        optics, false, glm::dvec3(0.0, 0.0, -1.0),
                        glm::dvec3(0.0, 0.0, 1.0), detail);
                }
                return;
            }

            glm::dmat4 opticsWorld = entityWorld * optics.pose;
            glm::dvec3 forward = -glm::dvec3(opticsWorld[2]);
            const double forwardLength = glm::length(forward);
            if (forwardLength <= 0.0)
            {
                cache.valid = false;
                for (ViewIDType viewID : vsgcontext->activeViewIDs)
                {
                    auto& detail = opticsDetails.views[viewID];
                    detail.focalDistance = optics.focalDistance;
                    detail.focalPointValid = false;
                    detail.autoComputeCacheValid = false;
                    updateClipDistances(
                        optics, false, glm::dvec3(0.0, 0.0, -1.0),
                        glm::dvec3(0.0, 0.0, 1.0), detail);
                }
                return;
            }
            forward /= forwardLength;

            const bool useCachedIntersection =
                canCache && cache.valid &&
                cache.terrainSRS == terrainSRS &&
                cache.lastTerrainRevision == terrainRevision &&
                matricesEqual(cache.lastOpticsWorld, opticsWorld);

            if (!useCachedIntersection)
            {
                cache.valid = false;
                cache.terrainSRS = terrainSRS;
                cache.lastOpticsWorld = opticsWorld;
                cache.lastTerrainRevision = terrainRevision;

                const glm::dvec3 origin(opticsWorld[3]);
                auto start = origin;
                if (optics.projection == Optics::Projection::Orthographic)
                    start -= forward * 1e6;
                const auto end = start + forward * 1e8;

                vsg::LineSegmentIntersector intersector(to_vsg(start), to_vsg(end));
                terrainNode->accept(intersector);
                if (!intersector.intersections.empty())
                {
                    auto closest = std::min_element(
                        intersector.intersections.begin(), intersector.intersections.end(),
                        [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

                    cache.focalPoint = to_glm(closest->get()->worldIntersection);
                    cache.focalDistance = glm::length(cache.focalPoint - origin);
                    cache.valid = true;
                }
            }

            glm::dvec3 terrainNormal(0.0, 0.0, 1.0);
            if (cache.valid && terrainSRS.isGeocentric())
            {
                terrainNormal = cache.focalPoint;
                const double normalLength = glm::length(terrainNormal);
                terrainNormal = normalLength > 0.0 ?
                    terrainNormal / normalLength : glm::dvec3(0.0, 0.0, 1.0);
            }

            for (ViewIDType viewID : vsgcontext->activeViewIDs)
            {
                auto& detail = opticsDetails.views[viewID];
                detail.focalDistance = cache.valid ? cache.focalDistance : optics.focalDistance;
                detail.focalPointValid = false;

                const auto& viewSRS = transformDetail.views[viewID].cache.world_srs;
                if (cache.valid && viewSRS.valid())
                {
                    auto focalPoint = GeoPoint(terrainSRS, cache.focalPoint).transform(viewSRS);
                    if (focalPoint.valid())
                    {
                        detail.focalPoint = { focalPoint.x, focalPoint.y, focalPoint.z };
                        detail.focalPointValid = true;
                    }
                }

                detail.autoComputeCacheValid = canCache && cache.valid;
                detail.lastTerrainRevision = terrainRevision;
                detail.lastAutoComputeWorld = opticsWorld;
                updateClipDistances(
                    optics, autoComputeNearFar, forward, terrainNormal, detail);
            }

            if (!canCache)
                cache.valid = false;
            return;
        }

        if (registry.any_of<TerrainOpticsCache>(entity))
            registry.remove<TerrainOpticsCache>(entity);

        for (ViewIDType viewID : vsgcontext->activeViewIDs)
        {
            auto& opticsDetail = opticsDetails.views[viewID];

            if (!autoComputeFocalDistance)
            {
                opticsDetail.focalDistance = optics.focalDistance;
                opticsDetail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
                opticsDetail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;
                opticsDetail.focalPointValid = false;
                opticsDetail.autoComputeCacheValid = false;
                continue;
            }

            if (!terrainTarget)
            {
                // Manual optics remain fully usable without a terrain target.
                // Auto-clamping falls back to the caller's explicit distances.
                opticsDetail.focalDistance = optics.focalDistance;
                opticsDetail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
                opticsDetail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;
                opticsDetail.focalPointValid = false;
                opticsDetail.autoComputeCacheValid = false;
                continue;
            }

            auto& transformView = transformDetail.views[viewID];
            if (transformView.revision < 0)
                continue;

            // Compose the optics' local pose onto the entity's world transform.
            glm::dmat4 entityWorld = to_glm(transformView.model);
            glm::dmat4 opticsWorld = entityWorld * optics.pose;

            const bool useCachedIntersection =
                canCache &&
                opticsDetail.autoComputeCacheValid &&
                opticsDetail.lastTerrainRevision == terrainRevision &&
                matricesEqual(opticsWorld, opticsDetail.lastAutoComputeWorld);

            // Forward axis in world space. Convention here is camera/projector looks down -Z.
            // Normalizing removes any scale contributed by Transform or pose.
            glm::dvec3 forward = -glm::dvec3(opticsWorld[2]);
            double fwdLen = glm::length(forward);
            if (fwdLen <= 0.0)
            {
                opticsDetail.focalDistance = optics.focalDistance;
                opticsDetail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
                opticsDetail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;
                opticsDetail.focalPointValid = false;
                opticsDetail.autoComputeCacheValid = false;
                continue;
            }

            forward /= fwdLen;

            if (!useCachedIntersection)
            {
                auto origin = vsg::dvec3(opticsWorld[3][0], opticsWorld[3][1], opticsWorld[3][2]);
                opticsDetail.focalPoint = to_glm(origin);
                opticsDetail.focalDistance = optics.focalDistance;
                opticsDetail.focalPointValid = false;

                // For orthographic projectors we can start well "behind" the projector
                // so a center that is slightly below terrain can still resolve a hit.
                auto start = origin;
                if (optics.projection == Optics::Projection::Orthographic)
                    start += to_vsg(forward * -1e6);

                auto end = start + to_vsg(forward * 1e8);

                vsg::LineSegmentIntersector lsi(start, end);
                terrainTarget->accept(lsi);

                const bool intersectionFound = !lsi.intersections.empty();
                if (intersectionFound)
                {
                    auto closest = std::min_element(
                        lsi.intersections.begin(), lsi.intersections.end(),
                        [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

                    opticsDetail.focalPoint = to_glm(closest->get()->worldIntersection);
                    opticsDetail.focalPointValid = true;
                    opticsDetail.focalDistance = glm::length(opticsDetail.focalPoint - to_glm(origin));
                }

                opticsDetail.lastAutoComputeWorld = opticsWorld;
                opticsDetail.lastTerrainRevision = terrainRevision;
                opticsDetail.autoComputeCacheValid = canCache && intersectionFound;
            }

            // Clip distances depend on user-adjustable optics parameters, so derive
            // them every update even when the terrain intersection remains cached.
            // A generic non-TerrainNode target has no SRS metadata, so retain the
            // original geocentric-normal assumption for this fallback path.
            glm::dvec3 terrainNormal = opticsDetail.focalPoint;
            const double normalLength = glm::length(terrainNormal);
            terrainNormal = normalLength > 0.0 ?
                terrainNormal / normalLength : glm::dvec3(0.0, 0.0, 1.0);
            updateClipDistances(
                optics, autoComputeNearFar, forward, terrainNormal, opticsDetail);
        }
    };

    registry.view<Optics, OpticsDetail, TransformDetail>().each(update);
}

void
OpticsSystemNode::initialize(VSGContext vsgcontext)
{
    updateTargetSubscription();
}

void
OpticsSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    updateTargetSubscription();
    updateOptics(vsgcontext);

    Inherit::update(vsgcontext);
}
