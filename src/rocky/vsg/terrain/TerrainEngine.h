/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/GeometryPool.h>
#include <rocky/vsg/terrain/TerrainState.h>
#include <rocky/vsg/terrain/TerrainTilePager.h>

namespace ROCKY_NAMESPACE
{
    class Map;

    /**
     * Access to all terrain-specific logic, data, and settings
     * associated with a Map.
     */
    class TerrainEngine
    {
    public:
        TerrainEngine(
            std::shared_ptr<Map> map,
            const SRS& worldSRS,
            VSGContext& runtime,
            const TerrainSettings& settings,
            TerrainTileHost* host);

        //! Terrain settings.
        const TerrainSettings& settings;

        //! runtime operations (scene graph, views, etc)
        VSGContext& context;

        //! the map this terrain is rendering
        std::shared_ptr<Map> map;

        //! SRS of the rendered terrain
        SRS worldSRS;

        //! Builds geometry for terrain tiles
        GeometryPool geometryPool;

        //! Tracks and updates state for terrain tiles
        TerrainTilePager tiles;

        //! Creates the state group objects for terrain rendering
        TerrainState stateFactory;

        //! name of job arena used to load data
        std::string loadSchedulerName = "rocky::terrain_loader";
    };
}
