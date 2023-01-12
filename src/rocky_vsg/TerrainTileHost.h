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

    /** 
     * Interface for terrain tiles to notify their host 
     * of their active state.
     */
    class TerrainTileHost
    {
    public:
        virtual void ping(
            TerrainTileNode* tile0,
            TerrainTileNode* tile1,
            TerrainTileNode* tile2,
            TerrainTileNode* tile3,
            vsg::RecordTraversal& nv) = 0;
    };
}
