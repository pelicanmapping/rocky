/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainEngine.h"
#include "TerrainTilePager.h"
#include "TerrainTileHost.h"
#include "TerrainSettings.h"
#include "GeometryPool.h"
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace ROCKY_NAMESPACE;


TerrainEngine::TerrainEngine(
    std::shared_ptr<Map> new_map,
    const Profile& new_profile,
    VSGContext& new_runtime,
    const TerrainSettings& new_settings,
    TerrainTileHost* host) :

    map(new_map),
    profile(new_profile),
    context(new_runtime),
    settings(new_settings),
    geometryPool(new_profile),
    tiles(new_profile, new_settings, new_runtime, host),
    stateFactory(new_runtime)
{
    ROCKY_SOFT_ASSERT(map, "Map is required");
    ROCKY_SOFT_ASSERT(profile.valid(), "Valid profile required");

    worldSRS = profile.srs().isGeodetic() ? profile.srs().geocentricSRS() : profile.srs();

    jobs::get_pool(loadSchedulerName)->set_concurrency(settings.concurrency);
}
