/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainContext.h"
#include "TileNodeRegistry.h"
#include "GeometryPool.h"
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace ROCKY_NAMESPACE;


TerrainContext::TerrainContext(
    shared_ptr<Map> new_map,
    const SRS& new_worldSRS,
    RuntimeContext& new_runtime,
    const TerrainSettings& new_settings,
    TerrainTileHost* host) :

    map(new_map),
    worldSRS(new_worldSRS),
    runtime(new_runtime),
    settings(new_settings)
{
    selectionInfo = std::make_shared<SelectionInfo>();
    selectionInfo->initialize(
        settings.firstLOD,
        settings.maxLOD,
        map->profile(),
        settings.minTileRangeFactor,
        true); // restruct polar subdivision..

    stateFactory = std::make_shared<StateFactory>();

    geometryPool = std::make_shared<GeometryPool>(
        worldSRS);

    tiles = std::make_shared<TileNodeRegistry>(host);
    tiles->geometryPool = geometryPool;
    tiles->selectionInfo = selectionInfo;
    tiles->stateFactory = stateFactory;

    util::job_scheduler::get(loadSchedulerName)->setConcurrency(4);
}
