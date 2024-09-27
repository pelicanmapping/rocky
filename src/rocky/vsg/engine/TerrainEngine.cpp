/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainEngine.h"
#include "TerrainTilePager.h"
#include "GeometryPool.h"
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace ROCKY_NAMESPACE;


TerrainEngine::TerrainEngine(
    shared_ptr<Map> new_map,
    const SRS& new_worldSRS,
    Runtime& new_runtime,
    const TerrainSettings& new_settings,
    TerrainTileHost* host) :

    map(new_map),
    worldSRS(new_worldSRS),
    runtime(new_runtime),
    settings(new_settings),
    geometryPool(worldSRS),
    tiles(new_map->profile(), new_settings, new_runtime, host),
    stateFactory(new_runtime)
{
    jobs::get_pool(loadSchedulerName)->set_concurrency(settings.concurrency);
}
