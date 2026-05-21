/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>

namespace ROCKY_NAMESPACE
{
    class TerrainTileNode;
    class TerrainTilePager;
    class TerrainSettings;

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

        virtual TerrainTilePager& tiles() = 0;

        //! Called when a tile enters residency, or its collision-relevant mesh changes.
        virtual void terrainTileBecameResident(TerrainTileNode*) { }

        //! Called before a tile leaves residency.
        virtual void terrainTileWillBeReleased(TerrainTileNode*) { }
    };
}
