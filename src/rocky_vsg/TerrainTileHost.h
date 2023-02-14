/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/nodes/QuadGroup.h>
#include <vsg/app/RecordTraversal.h>

namespace ROCKY_NAMESPACE
{
    class TerrainTileNode;
    class TerrainSettings;

    /** 
     * Interface for terrain tiles to notify their host of their active state.
     */
    class TerrainTileHost
    {
    public:
        //! Tell the host that a tile is alive!
        virtual void ping(
            TerrainTileNode* tile,
            bool parentHasData,
            vsg::RecordTraversal& t) = 0;

        //! Access terrain settings.
        virtual const TerrainSettings& settings() = 0;
    };
}
