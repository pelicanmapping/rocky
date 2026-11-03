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
    class TerrainTileFactory
    {
    public:
        TerrainTileFactory(
            std::shared_ptr<const Map> map,
            const Profile& profile,
            const SRS& renderingSRS,
            std::shared_ptr<TerrainState> state,
            VSGContext context,
            const TerrainSettings& settings,
            TerrainTileHost* host);

        ~TerrainTileFactory();

        //! Terrain settings.
        const TerrainSettings& settings;

        //! the map this terrain is rendering
        std::shared_ptr<const Map> map;

        //! tiling profile for querying the map
        Profile profile;

        //! SRS of the rendered terrain
        SRS renderingSRS;

        //! Builds geometry for terrain tiles
        GeometryPool geometryPool;

        //! Holds the state group objects for terrain rendering
        std::shared_ptr<TerrainState> state;

        TerrainTileHost* host = nullptr;

        //! name of job arena used to load data
        std::string loadSchedulerName = "rocky::terrain_loader";

        //! Create a single terrain tile; returns null if its descriptors fail to compile.
        vsg::ref_ptr<TerrainTileNode> createTile(
            const TileKey& key,
            vsg::ref_ptr<TerrainTileNode> parent,
            VSGContext context);

        bool update(VSGContext context);
    };
}
