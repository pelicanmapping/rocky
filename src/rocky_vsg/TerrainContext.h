/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky_vsg/Merger.h>
#include <rocky_vsg/RuntimeContext.h>
#include <rocky_vsg/SelectionInfo.h>
#include <rocky_vsg/StateFactory.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TileNodeRegistry.h>
#include <rocky_vsg/TileRenderModel.h>

namespace rocky
{
    class Map;

    /**
     * Access to all terrain-specific logic, data, and settings
     * associated with a Map.
     */
    class TerrainContext
    {
    public:
        TerrainContext(
            shared_ptr<Map> map,
            RuntimeContext& runtime,
            const TerrainSettings& settings,
            TerrainTileHost* host);

        //! Terrain settings.
        const TerrainSettings& settings;

        //! runtime operations (scene graph, views, etc)
        RuntimeContext& runtime;

        //! the map this terrain is rendering
        shared_ptr<Map> map;

        //! creator of terrain tile triangles and attributes
        shared_ptr<GeometryPool> geometryPool;

        //! merges new tiles into the live scene graph
        Merger merger;

        //! tracks all existing tiles
        shared_ptr<TileNodeRegistry> tiles;

        //! manages visibility and morphing ranges
        shared_ptr<SelectionInfo> selectionInfo;

        //! Factory for creating rendering state commands
        shared_ptr<StateFactory> stateFactory;

        //! name of job arena used to load data
        std::string loadArenaName = "terrain.load";
    };

}
