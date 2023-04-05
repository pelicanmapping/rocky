/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky_vsg/RuntimeContext.h>
#include <rocky_vsg/SelectionInfo.h>
#include <rocky_vsg/TerrainStateFactory.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TileNodeRegistry.h>

namespace ROCKY_NAMESPACE
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
            const SRS& worldSRS,
            RuntimeContext& runtime,
            const TerrainSettings& settings,
            TerrainTileHost* host);

        //! Terrain settings.
        const TerrainSettings& settings;

        //! runtime operations (scene graph, views, etc)
        RuntimeContext& runtime;

        //! the map this terrain is rendering
        shared_ptr<Map> map;

        //! SRS of the rendered terrain
        SRS worldSRS;

        //! creator of terrain tile triangles and attributes
        shared_ptr<GeometryPool> geometryPool;

        //! tracks all existing tiles
        shared_ptr<TileNodeRegistry> tiles;

        //! manages visibility and morphing ranges
        shared_ptr<SelectionInfo> selectionInfo;

        //! Factory for creating rendering state commands
        shared_ptr<TerrainStateFactory> stateFactory;

        //! name of job arena used to load data
        std::string loadSchedulerName = "terrain.load";
    };
}
