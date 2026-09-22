/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OpticsSystem.h"
#include "../terrain/TerrainNode.h"
#include <rocky/vsg/VSGUtils.h>
#include <algorithm>
#include <cmath>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    //! Compares projector matrices exactly because both values originate from
    //! the same synchronized Transform and Optics component state.
    bool matricesEqual(const glm::dmat4& lhs, const glm::dmat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (lhs[column][row] != rhs[column][row])
                    return false;
        return true;
    }

    //! Restores manual optical distances when terrain placement is unavailable
    //! or disabled. The caller supplies null optics for an implicit unit box.
    void setManualProjection(const Optics* optics, ProjectionViewDetail& detail)
    {
        detail.focalPointValid = false;
        detail.focalDistance = optics ? optics->focalDistance : 1.0;
        detail.nearDistance = optics ?
            optics->focalDistance * optics->nearScale + optics->nearBias : 1.0;
        detail.farDistance = optics ?
            optics->focalDistance * optics->farScale + optics->farBias : 1.0;
    }

    //! Fits perspective clip distances to a locally planar terrain surface at
    //! the center-ray hit. Grazing angles are bounded to avoid singular ranges.
    void fitPerspectiveClipRange(
        const Optics& optics,
        const glm::dvec3& forward,
        ProjectionViewDetail& detail)
    {
        glm::dvec3 terrainNormal = detail.focalPoint;
        double normalLength = glm::length(terrainNormal);
        if (normalLength > 0.0)
            terrainNormal /= normalLength;
        else
            terrainNormal = glm::dvec3(0.0, 0.0, 1.0);

        auto clamp01 = [](double value)
        {
            return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
        };

        double cosAlpha = std::max(1e-6, clamp01(glm::abs(glm::dot(forward, terrainNormal))));
        double alpha = std::acos(cosAlpha);
        double tanHalfY = std::tan(glm::radians(optics.fovY * 0.5));
        double tanHalfX = tanHalfY * optics.aspectRatio;
        double coneHalfAngle = std::atan(std::sqrt(tanHalfX * tanHalfX + tanHalfY * tanHalfY));
        double height = detail.focalDistance * cosAlpha;
        double nearAngle = alpha - coneHalfAngle;
        double farAngle = std::min(alpha + coneHalfAngle, glm::radians(89.0));
        double nearRaw = nearAngle <= 0.0 ?
            detail.focalDistance : height / std::max(std::cos(nearAngle), 1e-6);
        double farRaw = height / std::max(std::cos(farAngle), 1e-6);
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
}

void OpticsSystemNode::on_construct_ProjectedTexture(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ProjectionDetail>(e);
}

void OpticsSystemNode::on_destroy_ProjectedTexture(entt::registry& r, entt::entity e)
{
    r.remove<ProjectionDetail>(e);
}

OpticsSystemNode::OpticsSystemNode(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& r)
    {
        r.on_construct<Optics>().connect<&OpticsSystemNode::on_construct_Optics>(*this);
        r.on_destroy<Optics>().connect<&OpticsSystemNode::on_destroy_Optics>(*this);
        r.on_construct<ProjectedTexture>().connect<&OpticsSystemNode::on_construct_ProjectedTexture>(*this);
        r.on_destroy<ProjectedTexture>().connect<&OpticsSystemNode::on_destroy_ProjectedTexture>(*this);

        r.view<Optics>().each([&](auto entity, auto&)
        {
            (void)r.get_or_emplace<OpticsDetail>(entity);
        });
        r.view<ProjectedTexture>().each([&](auto entity, auto&)
        {
            (void)r.get_or_emplace<ProjectionDetail>(entity);
        });
    });
}

void OpticsSystemNode::updateTargetSubscription()
{
    auto currentTarget = target.ref_ptr();
    auto subscribedTarget = _subscribedTarget.ref_ptr();
    if (currentTarget == subscribedTarget)
        return;

    _terrainSubscriptions.clear();
    _subscribedTarget = currentTarget;
    _targetHasChangeNotifications = false;
    _targetChanged = true;
    {
        std::scoped_lock lock(_loadedTilesMutex);
        _loadedTiles.clear();
    }

    if (auto terrain = currentTarget.cast<TerrainNode>())
    {
        _targetHasChangeNotifications = true;
        _terrainSubscriptions += terrain->onTileLoaded([this](const TileKey& key)
        {
            // Tile callbacks can run on loader threads. Queue the key and let
            // the update thread invalidate only projections over that tile.
            std::scoped_lock lock(_loadedTilesMutex);
            _loadedTiles.insert(key);
        });
    }
}

void OpticsSystemNode::updateOptics(VSGContext vsgcontext)
{
    auto writer = _registry.write();
    auto& registry = writer.registry;

    registry.view<Optics, OpticsDetail>().each([&](auto, const auto& optics, auto& details)
    {
        for (ViewIDType viewID : vsgcontext->activeViewIDs)
        {
            auto& detail = details.views[viewID];
            detail.focalDistance = optics.focalDistance;
            detail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
            detail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;
        }
    });
}

void OpticsSystemNode::updateProjections(VSGContext vsgcontext)
{
    std::unordered_set<TileKey> loadedTiles;
    {
        std::scoped_lock lock(_loadedTilesMutex);
        loadedTiles.swap(_loadedTiles);
    }

    auto writer = _registry.write();
    auto& registry = writer.registry;
    auto terrainTarget = target.ref_ptr();

    if (_targetChanged || !loadedTiles.empty())
    {
        registry.view<ProjectionDetail>().each([&](auto, auto& projectionDetails)
        {
            for (auto& detail : projectionDetails.views)
            {
                if (_targetChanged)
                {
                    detail.intersectionCacheValid = false;
                    continue;
                }

                if (!detail.intersectionCacheValid || !detail.focalPointValid || !detail.lastWorldSRS.valid())
                    continue;

                GeoPoint hit(detail.lastWorldSRS, detail.focalPoint);
                for (const auto& key : loadedTiles)
                {
                    if (key.extent().contains(hit))
                    {
                        detail.intersectionCacheValid = false;
                        break;
                    }
                }
            }
        });
        _targetChanged = false;
    }

    const bool canCache = _targetHasChangeNotifications;
    registry.view<ProjectedTexture, ProjectionDetail>().each(
        [&](auto entity, const auto& projected, auto& projectionDetails)
    {
        if (auto* active = registry.try_get<ActiveState>(entity); active && !active->active)
            return;

        auto projector = projected.projector != entt::null ? projected.projector : entity;
        auto* transformDetails = registry.try_get<TransformDetail>(projector);
        if (!transformDetails)
            return;

        auto* optics = registry.try_get<Optics>(projector);
        auto* visibility = registry.try_get<Visibility>(entity);
        const auto projection = optics ? optics->projection : Optics::Projection::Orthographic;

        for (ViewIDType viewID : vsgcontext->activeViewIDs)
        {
            auto& detail = projectionDetails.views[viewID];
            auto& transformView = transformDetails->views[viewID];
            if (transformView.revision < 0)
                continue;
            if (visibility && !visibility->visible[viewID])
                continue;

            // Do not use the projector Transform's pre-placement cull result here.
            // Terrain placement can move an initially culled projector into view.
            if (projected.placement != ProjectionPlacement::Terrain || !terrainTarget)
            {
                setManualProjection(optics, detail);
                detail.intersectionCacheValid = false;
                continue;
            }

            glm::dmat4 projectorWorld = to_glm(transformView.model);
            if (optics)
                projectorWorld *= optics->pose;

            glm::dvec3 forward = -glm::dvec3(projectorWorld[2]);
            double forwardLength = glm::length(forward);
            if (forwardLength <= 0.0)
            {
                setManualProjection(optics, detail);
                detail.intersectionCacheValid = false;
                continue;
            }
            forward /= forwardLength;

            const bool useCachedIntersection =
                canCache &&
                detail.intersectionCacheValid &&
                matricesEqual(projectorWorld, detail.lastProjectorWorld);

            if (!useCachedIntersection)
            {
                auto origin = vsg::dvec3(projectorWorld[3][0], projectorWorld[3][1], projectorWorld[3][2]);
                setManualProjection(optics, detail);

                // An orthographic source fit can initially place its center below
                // terrain, so begin far behind the volume and probe along -Z.
                auto start = origin;
                if (projection == Optics::Projection::Orthographic)
                    start += to_vsg(forward * -1e6);
                auto end = start + to_vsg(forward * 1e8);

                vsg::LineSegmentIntersector intersector(start, end);
                terrainTarget->accept(intersector);
                const bool found = !intersector.intersections.empty();
                if (found)
                {
                    auto closest = std::min_element(
                        intersector.intersections.begin(), intersector.intersections.end(),
                        [](const auto& lhs, const auto& rhs)
                        {
                            return lhs->ratio < rhs->ratio;
                        });
                    detail.focalPoint = to_glm(closest->get()->worldIntersection);
                    detail.focalPointValid = true;
                    detail.focalDistance = glm::length(detail.focalPoint - to_glm(origin));
                }

                detail.lastProjectorWorld = projectorWorld;
                detail.lastWorldSRS = transformView.cache.world_srs;
                detail.intersectionCacheValid = canCache && found;
            }

            detail.nearDistance = optics ?
                detail.focalDistance * optics->nearScale + optics->nearBias : 1.0;
            detail.farDistance = optics ?
                detail.focalDistance * optics->farScale + optics->farBias : 1.0;

            if (optics &&
                projection == Optics::Projection::Perspective &&
                projected.computeClipRange &&
                detail.focalPointValid)
            {
                fitPerspectiveClipRange(*optics, forward, detail);
            }
        }
    });
}

void OpticsSystemNode::initialize(VSGContext)
{
    updateTargetSubscription();
}

void OpticsSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    updateTargetSubscription();
    updateOptics(vsgcontext);
    updateProjections(vsgcontext);
    Inherit::update(vsgcontext);
}
