/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/app/RecordTraversal.h>

namespace ROCKY_NAMESPACE
{
    class TerrainTileNode;
    class TerrainSettings;

    /** 
     * Interface for terrain tiles to notify their host 
     * of their active state.
     */
    class TerrainTileHost
    {
    public:
        //! Tell the host that a group of tiles are still alive.
        virtual void ping(
            const TerrainTileNode* parent,
            TerrainTileNode* tile0,
            TerrainTileNode* tile1,
            TerrainTileNode* tile2,
            TerrainTileNode* tile3,
            vsg::RecordTraversal& nv) = 0;

        //! Access terrain settings.
        virtual const TerrainSettings& settings() = 0;
    };
}
