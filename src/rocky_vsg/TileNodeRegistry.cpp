/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileNodeRegistry.h"
#include "TerrainSettings.h"
#include <vsg/ui/FrameStamp.h>

using namespace rocky;

#define LC "[TileNodeRegistry] "

//----------------------------------------------------------------------------

TileNodeRegistry::TileNodeRegistry() :
    _notifyNeighbors(false),
    _mutex("TileNodeRegistry(OE)")
{
    //nop
}

TileNodeRegistry::~TileNodeRegistry()
{
    releaseAll();
}

void
TileNodeRegistry::setNotifyNeighbors(bool value)
{
    _notifyNeighbors = value;
}

void
TileNodeRegistry::setDirty(
    const GeoExtent& extent,
    unsigned minLevel,
    unsigned maxLevel,
    const CreateTileManifest& manifest,
    TerrainContext& terrain)
{
    util::ScopedLock lock(_mutex);
    
    for(auto& [key, entry] : _tiles)
    //for( TileTable::iterator i = _tiles.begin(); i != _tiles.end(); ++i )
    {
        //const TileKey& key = i->first;

        if (minLevel <= key.getLOD() && 
            maxLevel >= key.getLOD() &&
            (!extent.valid() || extent.intersects(key.getExtent())))
        {
            entry._tile->refreshLayers(manifest);
            //i->second._tile->refreshLayers(manifest);
        }
    }
}

void
TileNodeRegistry::add(
    TerrainTileNode* tile,
    TerrainContext& terrain)
{
    util::ScopedLock lock(_mutex);

    auto& entry = _tiles[tile->getKey()];
    entry._tile = tile;
    bool recyclingOrphan = entry._trackerToken != nullptr;
    entry._trackerToken = _tracker.use(tile, nullptr);

    // Start waiting on our neighbors.
    // (If we're recycling and orphaned record, we need to remove old listeners first)
    if (_notifyNeighbors)
    {
        const TileKey& key = tile->getKey();

        // If we're recycling, we need to remove the old listeners first
        if (recyclingOrphan)
        {
            stopListeningFor(key.createNeighborKey(1, 0), key, terrain);
            stopListeningFor(key.createNeighborKey(0, 1), key, terrain);
        }

        startListeningFor(key.createNeighborKey(1, 0), tile, terrain);
        startListeningFor(key.createNeighborKey(0, 1), tile, terrain);

        // check for tiles that are waiting on this tile, and notify them!
        TileKeyOneToMany::iterator notifier = _notifiers.find( tile->getKey() );
        if ( notifier != _notifiers.end() )
        {
            TileKeySet& listeners = notifier->second;

            for(TileKeySet::iterator listener = listeners.begin(); listener != listeners.end(); ++listener)
            {
                TileTable::iterator i = _tiles.find( *listener );
                if ( i != _tiles.end())
                {
                    i->second._tile->notifyOfArrival(tile, terrain);
                }
            }
            _notifiers.erase( notifier );
        }
    }

    ROCKY_INFO << LC << "Tiles = " << _tiles.size() << std::endl;
}

void
TileNodeRegistry::startListeningFor(
    const TileKey& tileToWaitFor,
    TerrainTileNode* waiter,
    TerrainContext& terrain)
{
    // ASSUME EXCLUSIVE LOCK

    TileTable::iterator i = _tiles.find(tileToWaitFor);
    if (i != _tiles.end())
    {
        TerrainTileNode* tile = i->second._tile.get();

        ROCKY_DEBUG << LC << waiter->getKey().str() << " listened for " << tileToWaitFor.str()
            << ", but it was already in the repo.\n";

        waiter->notifyOfArrival(tile, terrain);
    }
    else
    {
        ROCKY_DEBUG << LC << waiter->getKey().str() << " listened for " << tileToWaitFor.str() << ".\n";
        _notifiers[tileToWaitFor].insert( waiter->getKey() );
    }
}

void
TileNodeRegistry::stopListeningFor(
    const TileKey& tileToWaitFor,
    const TileKey& waiterKey,
    TerrainContext& terrain)
{
    // ASSUME EXCLUSIVE LOCK

    TileKeyOneToMany::iterator i = _notifiers.find(tileToWaitFor);
    if (i != _notifiers.end())
    {
        // remove the waiter from this set:
        i->second.erase(waiterKey);

        // if the set is now empty, remove the set entirely
        if (i->second.empty())
        {
            _notifiers.erase(i);
        }
    }
}

void
TileNodeRegistry::releaseAll()
{
    util::ScopedLock lock(_mutex);

    for (auto& tile : _tiles)
    {
        //tile.second._tile->releaseGLObjects(state);
    }

    _tiles.clear();

    _tracker.reset();

    _notifiers.clear();

    _tilesToUpdate.clear();

    //OE_PROFILING_PLOT(PROFILING_REX_TILES, (float)(_tiles.size()));
}

void
TileNodeRegistry::touch(const TerrainTileNode* tile)
{
    util::ScopedLock lock(_mutex);

    TileTable::iterator i = _tiles.find(tile->getKey());

    ROCKY_SOFT_ASSERT_AND_RETURN(i != _tiles.end(), void());

    _tracker.use(
        const_cast<TerrainTileNode*>(tile),
        i->second._trackerToken);

    if (tile->updateRequired())
    {
        _tilesToUpdate.push_back(tile->getKey());
    }
}

void
TileNodeRegistry::update(const TerrainSettings& settings, const vsg::FrameStamp* fs)
{
    util::ScopedLock lock(_mutex);

    if (!_tilesToUpdate.empty())
    {
        // Sorting these from high to low LOD will reduce the number 
        // of inheritance steps each updated image will have to perform
        // against the tile's children
        std::sort(_tilesToUpdate.begin(), _tilesToUpdate.end(),
            [](const TileKey& lhs, const TileKey& rhs) {
                return lhs.getLOD() > rhs.getLOD();
            });

        for (auto& key : _tilesToUpdate)
        {
            auto iter = _tiles.find(key);
            if (iter != _tiles.end())
            {
                iter->second._tile->update(fs);
            }
        }

        _tilesToUpdate.clear();
    }

    //collectDormantTiles(settings, fs);
}

void
TileNodeRegistry::collectDormantTiles(
    const TerrainSettings& settings,
    const vsg::FrameStamp* fs)
    //std::chrono::steady_clock::time_point oldestAllowableTime,
    //unsigned oldestAllowableFrame,
    //float farthestAllowableRange,
    //unsigned maxTiles)
    //TerrainContext& terrain,
    //std::vector<vsg::observer_ptr<TerrainTileNode>>& output)
{
    //util::ScopedLock lock(_mutex);

    unsigned count = 0u;


    auto oldestAllowableTime = std::chrono::steady_clock::now() - std::chrono::seconds(settings.minSecondsBeforeUnload);
    auto oldestAllowableFrame = fs->frameCount - settings.minFramesBeforeUnload;

    const auto disposeTile = [&](TerrainTileNode* tile) -> bool
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(tile != nullptr, true);

        const TileKey& key = tile->getKey();

        if (tile->getDoNotExpire() == false &&
            tile->getLastTraversalTime() < oldestAllowableTime &&
            tile->getLastTraversalFrame() < (int)oldestAllowableFrame &&
            tile->getLastTraversalRange() > settings.minRangeBeforeUnload) // &&
            //tile->areSiblingsDormant(terrain))
        {
            //if (_notifyNeighbors)
            //{
            //    // remove neighbor listeners:
            //    stopListeningFor(key.createNeighborKey(1, 0), key, terrain);
            //    stopListeningFor(key.createNeighborKey(0, 1), key, terrain);
            //}

            //output.push_back(vsg::observer_ptr<TerrainTileNode>(tile));

            _tiles.erase(key);

            return true; // dispose it
        }
        else
        {
            tile->resetTraversalRange();

            return false; // keep it
        }
    };
    
    _tracker.flush(settings.maxTilesToUnloadPerFrame, disposeTile);

    //ROCKY_PROFILING_PLOT(PROFILING_REX_TILES, (float)(_tiles.size()));
}
