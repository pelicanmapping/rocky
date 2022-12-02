/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Unloader.h"

#undef  LC
#define LC "[Unloader] "

using namespace rocky;


Unloader::Unloader() :
    _minResidentTiles(0u),
    _maxAge(100),
    _minRange(0.0f),
    _maxTilesToUnloadPerFrame(~0),
    _frameLastUpdated(0u)
{
    //ADJUST_UPDATE_TRAV_COUNT(this, +1);
}

void
Unloader::update(TerrainContext& terrain)
{
    unsigned frame = 0;
    ROCKY_TODO("Pass in the frame number");

    if (terrain.tiles.size() > _minResidentTiles)
    {
        //_frameLastUpdated = frame;

        auto now = clock::now();

        unsigned count = 0u;

        // Have to enforce both the time delay AND a frame delay since the frames can
        // stop while the time rolls on (e.g., if you are dragging the window)
        clock::time_point oldestAllowableTime = now - _maxAge;
        unsigned oldestAllowableFrame = std::max(frame, 3u) - 3u;

        // Remove them from the registry:
        terrain.tiles.collectDormantTiles(
            oldestAllowableTime,
            oldestAllowableFrame,
            _minRange,
            _maxTilesToUnloadPerFrame,
            terrain,
            _deadpool);

        // Remove them from the scene graph:
        for (auto& tile_weakptr : _deadpool)
        {
            // may be NULL since we're removing scene graph objects as we go!
            vsg::ref_ptr<TerrainTileNode> tile(tile_weakptr);
            if (tile.valid())
            {
                auto parent = tile->getParentTile();

                // Check that this tile doesn't have any live quadtree siblings. If it does,
                // we don't want to remove them too!
                // GW: moved this check to the collectAbandonedTiles function where it belongs
                if (parent.valid())
                {
                    parent->removeSubTiles();
                    ++count;
                }
            }
        }

        if (_deadpool.empty() == false)
        {
            ROCKY_DEBUG << LC
                << "Unloaded " << count << " of "
                << _deadpool.size() << " dormant tiles; "
                << terrain.tiles.size() << " remain active."
                << std::endl;
        }

        _deadpool.clear();
    }
}

