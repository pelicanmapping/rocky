/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainContext.h"
#include "TileNodeRegistry.h"
#include "GeometryPool.h"
#include <rocky/Notify.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace rocky;


TerrainContext::TerrainContext(
    shared_ptr<Map> new_map,
    RuntimeContext& new_runtime,
    const TerrainSettings& new_settings,
    TerrainTileHost* host) :

    map(new_map),
    runtime(new_runtime),
    settings(new_settings)
{
    selectionInfo = std::make_shared<SelectionInfo>();
    selectionInfo->initialize(
        settings.firstLOD,
        settings.maxLOD,
        map->getProfile(),
        settings.minTileRangeFactor,
        true); // restruct polar subdivision..

    stateFactory = std::make_shared<StateFactory>();

    geometryPool = std::make_shared<GeometryPool>();

    tiles = std::make_shared<TileNodeRegistry>(host);
    tiles->geometryPool = geometryPool;
    tiles->selectionInfo = selectionInfo;
    tiles->stateFactory = stateFactory;
}

void
TerrainContext::loadAndMergeData(
    TerrainTileNode* tile,
    std::function<TerrainTileModel(Cancelable*)> loader)
{
}