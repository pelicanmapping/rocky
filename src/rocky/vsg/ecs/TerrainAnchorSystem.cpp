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
        GeoPoint location;
        GeoPoint terrainPoint;
        bool queryCacheValid = false;
        bool terrainPointValid = false;
    };
}

namespace
{
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
        if (!transform.position.valid())
            return;

        auto location = transform.position.transform(geodeticSRS);
        if (!location.valid())
            return;
        location.z = 0.0;

        if (!sameHorizontalLocation(location, detail.location))
            detail.queryCacheValid = false;

        if (!detail.queryCacheValid)
        {
            detail.location = location;
            detail.terrainPointValid = false;

            auto intersection = terrain->intersect(transform.position);
            if (intersection.ok())
            {
                detail.terrainPoint = intersection.value().point;
                detail.terrainPointValid = true;
            }
            detail.queryCacheValid = true;
        }

        if (!detail.terrainPointValid || !std::isfinite(anchor.offset))
            return;

        auto resolvedGeodetic = detail.terrainPoint.transform(geodeticSRS);
        if (!resolvedGeodetic.valid())
            return;
        resolvedGeodetic.z += anchor.offset;

        auto resolved = resolvedGeodetic.transform(transform.position.srs);
        auto currentWorld = transform.position.transform(terrain->renderingSRS);
        auto resolvedWorld = resolved.transform(terrain->renderingSRS);
        if (!resolved.valid() || !currentWorld.valid() || !resolvedWorld.valid())
            return;

        if (glm::length(glm::dvec3(resolvedWorld) - glm::dvec3(currentWorld)) > 0.001)
        {
            transform.position = resolved;
            transform.dirty(registry);
        }
    });
}
