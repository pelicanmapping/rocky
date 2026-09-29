/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TerrainAnchorSystem.h"
#include "../terrain/TerrainNode.h"
#include <rocky/ecs/Transform.h>
#include <cmath>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace ROCKY_NAMESPACE::detail
{
    //! Per-anchor terrain-query cache, invalidated by motion or relevant tile loads.
    struct TerrainAnchorDetail
    {
        // Requested longitude/latitude is authoritative; never replace it with an intersection's XY.
        GeoPoint location;
        SRS renderingSRS;
        double terrainHeight = 0.0;
        bool terrainHeightValid = false;
        bool queryCacheValid = false;

        // Recognize our own writes exactly, avoiding SRS round-trip noise and redundant queries/dirty notifications.
        GeoPoint lastPosition;
        double lastOffset = 0.0;
    };
}

namespace
{
    //! Tests validity and finiteness; GeoPoint::valid alone only validates its SRS.
    bool finitePoint(const GeoPoint& point)
    {
        return point.valid() && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }

    //! Returns whether two geodetic points identify the same horizontal location.
    bool sameHorizontalLocation(const GeoPoint& lhs, const GeoPoint& rhs)
    {
        constexpr double epsilon = 1e-12;
        return
            lhs.valid() &&
            rhs.valid() &&
            lhs.srs == rhs.srs &&
            std::abs(lhs.x - rhs.x) <= epsilon &&
            std::abs(lhs.y - rhs.y) <= epsilon;
    }
}

void TerrainAnchorSystem::on_construct_TerrainAnchor(entt::registry& registry, entt::entity entity)
{
    (void)registry.get_or_emplace<TerrainAnchorDetail>(entity);
}

void TerrainAnchorSystem::on_destroy_TerrainAnchor(entt::registry& registry, entt::entity entity)
{
    registry.remove<TerrainAnchorDetail>(entity);
}

TerrainAnchorSystem::TerrainAnchorSystem(Registry& registry) :
    System(registry)
{
    registry.write([&](entt::registry& reg)
    {
        reg.on_construct<TerrainAnchor>().connect<&TerrainAnchorSystem::on_construct_TerrainAnchor>(*this);
        reg.on_destroy<TerrainAnchor>().connect<&TerrainAnchorSystem::on_destroy_TerrainAnchor>(*this);

        reg.view<TerrainAnchor>().each([&](auto entity, auto&)
        {
            (void)reg.get_or_emplace<TerrainAnchorDetail>(entity);
        });
    });
}

TerrainAnchorSystem::~TerrainAnchorSystem()
{
    _terrainSubscriptions.clear();
    _registry.write([&](entt::registry& reg)
    {
        reg.on_construct<TerrainAnchor>().disconnect<&TerrainAnchorSystem::on_construct_TerrainAnchor>(*this);
        reg.on_destroy<TerrainAnchor>().disconnect<&TerrainAnchorSystem::on_destroy_TerrainAnchor>(*this);
    });
}

void TerrainAnchorSystem::updateTargetSubscription()
{
    auto currentTarget = target.ref_ptr();
    auto subscribedTarget = _subscribedTarget.ref_ptr();
    if (currentTarget == subscribedTarget)
        return;

    _terrainSubscriptions.clear();
    _subscribedTarget = currentTarget;
    _targetChanged = true;
    {
        std::scoped_lock lock(_loadedTilesMutex);
        _loadedTiles.clear();
    }

    if (currentTarget)
    {
        _terrainSubscriptions += currentTarget->onTileLoaded([this](const TileKey& key)
        {
            // Loader threads only enqueue keys; cache mutation stays on the update thread.
            std::scoped_lock lock(_loadedTilesMutex);
            _loadedTiles.insert(key);
        });
    }
}

void TerrainAnchorSystem::update(VSGContext vsgcontext)
{
    (void)vsgcontext;
    updateTargetSubscription();

    auto terrain = target.ref_ptr();
    if (!terrain || !terrain->renderingSRS.valid())
        return;

    std::unordered_set<TileKey> loadedTiles;
    {
        std::scoped_lock lock(_loadedTilesMutex);
        loadedTiles.swap(_loadedTiles);
    }

    auto writer = _registry.write();
    auto& registry = writer.registry;

    if (_targetChanged || !loadedTiles.empty())
    {
        registry.view<TerrainAnchorDetail>().each([&](auto& detail)
        {
            if (_targetChanged)
            {
                detail.queryCacheValid = false;
                return;
            }

            if (!detail.queryCacheValid || !detail.location.valid())
                return;

            for (const auto& key : loadedTiles)
            {
                if (key.extent().contains(detail.location))
                {
                    detail.queryCacheValid = false;
                    break;
                }
            }
        });
        _targetChanged = false;
    }

    const auto geodeticSRS = terrain->renderingSRS.geodeticSRS();
    registry.view<TerrainAnchor, Transform, TerrainAnchorDetail>().each(
        [&](auto, const auto& anchor, auto& transform, auto& detail)
    {
        if (!finitePoint(transform.position) || !std::isfinite(anchor.offset))
            return;

        const bool samePosition = transform.position == detail.lastPosition;
        const bool sameRenderingSRS = terrain->renderingSRS == detail.renderingSRS;
        if (detail.queryCacheValid && samePosition && sameRenderingSRS && anchor.offset == detail.lastOffset)
            return;

        auto currentGeodetic = transform.position.transform(geodeticSRS);
        if (!finitePoint(currentGeodetic))
            return;

        if (!sameRenderingSRS || (!samePosition && !sameHorizontalLocation(currentGeodetic, detail.location)))
        {
            detail.location = currentGeodetic;
            detail.location.z = 0.0;
            detail.queryCacheValid = false;
        }

        if (!detail.queryCacheValid)
        {
            detail.renderingSRS = terrain->renderingSRS;
            detail.terrainHeightValid = false;

            auto intersection = terrain->intersectVertical(detail.location);
            if (intersection.ok())
            {
                auto terrainPoint = intersection.value().point.transform(geodeticSRS);
                if (finitePoint(terrainPoint))
                {
                    detail.terrainHeight = terrainPoint.z;
                    detail.terrainHeightValid = true;
                }
            }
            // Cache misses too: only motion, a different target/SRS, or a relevant tile load warrants another query.
            detail.queryCacheValid = true;
        }

        if (detail.terrainHeightValid)
        {
            auto resolvedGeodetic = detail.location;
            resolvedGeodetic.z = detail.terrainHeight + anchor.offset;
            auto resolved = resolvedGeodetic.transform(transform.position.srs);
            if (!finitePoint(resolved))
                return;

            // Preserve caller-authored XY exactly in geographic/projected coordinates. ECEF requires all three
            // Cartesian coordinates to change with height; its horizontal invariant is the cached longitude/latitude.
            if (!transform.position.srs.isGeocentric())
            {
                resolved.x = transform.position.x;
                resolved.y = transform.position.y;
            }

            // Geodetic height is in meters, even when the Transform uses a projection with different map units.
            if (std::abs(resolvedGeodetic.z - currentGeodetic.z) > 0.001)
            {
                transform.position = resolved;
                transform.dirty(registry);
            }
        }

        detail.lastPosition = transform.position;
        detail.lastOffset = anchor.offset;
    });
}
