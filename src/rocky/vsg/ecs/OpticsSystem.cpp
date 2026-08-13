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
    bool matricesEqual(const glm::dmat4& lhs, const glm::dmat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (lhs[column][row] != rhs[column][row])
                    return false;
        return true;
    }
}

void OpticsSystemNode::on_construct_Optics(entt::registry& r, entt::entity e)
{
    r.emplace<OpticsDetail>(e);
}

void OpticsSystemNode::on_destroy_Optics(entt::registry& r, entt::entity e)
{
    r.remove<OpticsDetail>(e);
}

OpticsSystemNode::OpticsSystemNode(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<Optics>().connect<&OpticsSystemNode::on_construct_Optics>(*this);
            r.on_destroy<Optics>().connect<&OpticsSystemNode::on_destroy_Optics>(*this);
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
    ROCKY_SOFT_ASSERT_AND_RETURN(target, void());

    const auto terrainRevision = _terrainRevision.load(std::memory_order_relaxed);
    const bool canCache = _targetHasChangeNotifications;

    auto update = [&](auto entity, auto& optics, auto& opticsDetails, auto& transformDetail)
    {
        for (ViewIDType viewID : vsgcontext->activeViewIDs)
        {
            auto& opticsDetail = opticsDetails.views[viewID];

            if (!optics.autoComputeFocalDistance)
            {
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
                target.ref_ptr()->accept(lsi);

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
            opticsDetail.nearDistance = opticsDetail.focalDistance * optics.nearScale + optics.nearBias;
            opticsDetail.farDistance = opticsDetail.focalDistance * optics.farScale + optics.farBias;

            if (optics.projection == Optics::Projection::Perspective &&
                optics.autoComputeNearFar &&
                opticsDetail.focalPointValid)
            {
                // TODO: fix this hack and use a real up vector
                // Terrain normal = local geocentric UP at the focal point.
                glm::dvec3 terrainNormal = opticsDetail.focalPoint;
                double nlen = glm::length(terrainNormal);
                if (nlen > 0.0)
                    terrainNormal /= nlen;
                else
                    terrainNormal = glm::dvec3(0.0, 0.0, 1.0);

                auto clamp01 = [](double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); };

                // alpha: center-ray angle from terrain normal (0=head-on, pi/2=grazing)
                double cosAlpha = clamp01(glm::abs(glm::dot(forward, terrainNormal)));
                cosAlpha = std::max(1e-6, cosAlpha);
                double alpha = acos(cosAlpha);

                // Frustum diagonal half-angle from center ray to a corner ray.
                double tanHalfY = tan(glm::radians(optics.fovY * 0.5));
                double tanHalfX = tanHalfY * optics.aspectRatio;
                double coneHalfAngle = atan(std::sqrt(tanHalfX * tanHalfX + tanHalfY * tanHalfY));

                // Eye-to-plane distance along terrain normal.
                double h = opticsDetail.focalDistance * cosAlpha;

                // Corner-ray incidence limits.
                double nearAngle = alpha - coneHalfAngle;
                double farAngle = alpha + coneHalfAngle;

                // Keep far angle away from 90 deg singularity.
                const double maxAngle = glm::radians(89.0);
                if (farAngle > maxAngle) farAngle = maxAngle;

                // Distance along ray to hit flat tangent plane.
                // If nearAngle < 0, center ray is the closest valid hit.
                double nearRaw =
                    (nearAngle <= 0.0) ? opticsDetail.focalDistance : (h / std::max(cos(nearAngle), 1e-6));
                double farRaw = h / std::max(cos(farAngle), 1e-6);

                // Padding and hard floors for stable clipping.
                double nearPad = std::max(1.0, opticsDetail.focalDistance * 0.05);
                double farPad = std::max(1.0, opticsDetail.focalDistance * 0.01);
                double nearClip = std::max(1.0, nearRaw - nearPad);
                double farClip = std::max(nearClip + 1.0, farRaw + farPad);

                // Store ABSOLUTE distances (not offsets).
                opticsDetail.nearDistance = nearClip * optics.nearScale + optics.nearBias;
                opticsDetail.farDistance = farClip * optics.farScale + optics.farBias;
            }
        }
    };

    _registry.write()->view<Optics, OpticsDetail, TransformDetail>().each(update);
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
