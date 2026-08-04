/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/TileKey.h>

namespace ROCKY_NAMESPACE
{
    class TerrainTileNode;
    class TerrainTilePager;
    class TerrainSettings;


    /**
     * Activity interface for getting terrain runtime information and metrics
     */
    class TerrainActivity
    {
    public:
        Callback<const TileKey> onTileLoaded;
    };


    /** 
     * Interface for terrain tiles to notify their host of their active state.
     */
    class TerrainTileHost
    {
    public:
        //! Tell the host that a tile is alive!
        virtual void ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& t) = 0;

        //! Access terrain settings.
        virtual const TerrainSettings& settings() const = 0;

        //! Access terrain activity interface.
        virtual TerrainActivity& activity() = 0;

        //! Access the tile pager.
        virtual TerrainTilePager& tiles() = 0;
    };
}
