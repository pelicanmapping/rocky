/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileNodeRegistry.h"
#include "TerrainSettings.h"
#include "TerrainContext.h"
#include "RuntimeContext.h"

#include <vsg/ui/FrameStamp.h>

using namespace rocky;

#define LC "[TileNodeRegistry] "

//----------------------------------------------------------------------------

TileNodeRegistry::TileNodeRegistry(TerrainTileHost* in_host) :
    _host(in_host),
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
    shared_ptr<TerrainContext> terrain)
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
    shared_ptr<TerrainContext> terrain)
{
    util::ScopedLock lock(_mutex);

    auto& entry = _tiles[tile->key];
    entry._tile = tile;
    bool recyclingOrphan = entry._trackerToken != nullptr;
    entry._trackerToken = _tracker.use(tile, nullptr);

#if 0
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
#endif

    //ROCKY_INFO << LC << "Tiles = " << _tiles.size() << std::endl;
}

void
TileNodeRegistry::startListeningFor(
    const TileKey& tileToWaitFor,
    TerrainTileNode* waiter,
    shared_ptr<TerrainContext> terrain)
{
    // ASSUME EXCLUSIVE LOCK

    TileTable::iterator i = _tiles.find(tileToWaitFor);
    if (i != _tiles.end())
    {
        TerrainTileNode* tile = i->second._tile.get();

        ROCKY_DEBUG << LC << waiter->key.str() << " listened for " << tileToWaitFor.str()
            << ", but it was already in the repo.\n";

        waiter->notifyOfArrival(tile, terrain);
    }
    else
    {
        ROCKY_DEBUG << LC << waiter->key.str() << " listened for " << tileToWaitFor.str() << ".\n";
        _notifiers[tileToWaitFor].insert( waiter->key );
    }
}

void
TileNodeRegistry::stopListeningFor(
    const TileKey& tileToWaitFor,
    const TileKey& waiterKey,
    shared_ptr<TerrainContext> terrain)
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

    _needsUpdate.clear();
    _needsData.clear();
    _needsChildren.clear();

    //OE_PROFILING_PLOT(PROFILING_REX_TILES, (float)(_tiles.size()));
}

void
TileNodeRegistry::ping(
    TerrainTileNode* tile0,
    TerrainTileNode* tile1,
    TerrainTileNode* tile2,
    TerrainTileNode* tile3,
    vsg::RecordTraversal& nv)
{
    util::ScopedLock lock(_mutex);

    for (auto tile : { tile0, tile1, tile2, tile3 })
    {
        if (tile)
        {
            TileTable::iterator i = _tiles.find(tile->key);

            if (i == _tiles.end())
            {
                // new entry:
                auto& entry = _tiles[tile->key];
                entry._tile = tile;
                bool recyclingOrphan = entry._trackerToken != nullptr;
                entry._trackerToken = _tracker.use(tile, nullptr);
            }
            else
            {
                _tracker.use(
                    const_cast<TerrainTileNode*>(tile),
                    i->second._trackerToken);
            }

            if (tile->_needsChildren)
                _needsChildren.push_back(tile->key);

            if (tile->_needsData)
                _needsData.push_back(tile->key);

            if (tile->_needsUpdate)
                _needsUpdate.push_back(tile->key);
        }
    }
}

void
TileNodeRegistry::update(
    const vsg::FrameStamp* fs,
    shared_ptr<TerrainContext> terrain)
{
    util::ScopedLock lock(_mutex);

    // update any tiles that asked for it
    for (auto& key : _needsUpdate)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            iter->second._tile->update(fs);
            //iter->second._tile->_needsUpdate = false;
        }
    }
    _needsUpdate.clear();

    // launch any "new children" requests
    for (auto& key : _needsChildren)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            createTileChildren(
                iter->second._tile.get(), // parent
                terrain);  // context

            iter->second._tile->_needsChildren = false;
        }
    }
    _needsChildren.clear();

    // launch any data loading requests
    for (auto& key : _needsData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestTileData(iter->second._tile.get(), terrain);
        }
    }
    _needsData.clear();

    // Flush unused tiles (i.e., tiles that failed to ping) out of
    // the system. Tiles ping their children all at once; this
    // should in theory prevent a child from expiring without its
    // siblings.
    // TODO: track frames, times, and resident requirements so we 
    // are not thrashing tiles in and out of memory. Perhaps a simple
    // L2 cache of disposed tiles would be appropriate instead of
    // all these limits.
    const auto dispose = [&](TerrainTileNode* tile)
    {
        if (!tile->doNotExpire)
        {
            tile->unloadChildren();
            _tiles.erase(tile->key);
            return true;
        }
        return false;
    };

    _tracker.flush(~0, dispose);

    //ROCKY_INFO << "tiles = " << _tiles.size() << std::endl;
}

#if 0
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

        const TileKey& key = tile->key;

        if (tile->doNotExpire == false &&
            (vsg::time_point)tile->lastTraversalTime < oldestAllowableTime &&
            tile->lastTraversalFrame < (int)oldestAllowableFrame &&
            tile->lastTraversalRange > settings.minRangeBeforeUnload) // &&
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
#endif

vsg::ref_ptr<TerrainTileNode>
TileNodeRegistry::createTile(
    const TileKey& key,
    TerrainTileNode* parent,
    shared_ptr<TerrainContext> terrain)
{
    GeometryPool::Settings geomSettings
    {
        terrain->settings.tileSize,
        terrain->settings.skirtRatio,
        terrain->settings.morphTerrain
    };

    // Get a shared geometry from the pool that corresponds to this tile key:
    auto geometry = terrain->geometryPool->getPooledGeometry(
        key,
        geomSettings,
        nullptr);

    // initialize all the per-tile uniforms the shaders will need:
    float range, morphStart, morphEnd;
    terrain->selectionInfo->get(key, range, morphStart, morphEnd);

    float one_over_end_minus_start = 1.0f / (morphEnd - morphStart);
    fvec2 morphConstants = fvec2(morphEnd * one_over_end_minus_start, one_over_end_minus_start);

    unsigned numLODs = terrain->selectionInfo->getNumLODs();

    // Make a tilekey to use for testing whether to subdivide.
    float childrenVisibilityRange = FLT_MAX;
    if (key.getLOD() < (terrain->selectionInfo->getNumLODs() - 1))
    {
        auto[tw, th] = key.getProfile()->getNumTiles(key.getLOD());
        TileKey testKey = key.createChildKey((key.getTileY() <= th / 2) ? 0 : 3);
        childrenVisibilityRange = terrain->selectionInfo->getRange(testKey);
    }

    // Make the new terrain tile
    auto tile = TerrainTileNode::create(
        key,
        parent,
        geometry,
        morphConstants,
        childrenVisibilityRange,
        terrain->stateFactory->defaultTileDescriptors,
        terrain->tiles->_host);


    //tile->setDoNotExpire(key.getLOD() == terrain.firstLOD);

    // set parent?
    // tile->setParent(parent) ... ?

    // Generate its state group:
    terrain->stateFactory->refreshStateGroup(tile);

    return tile;
}
    
#if 0
vsg::ref_ptr<TerrainTileNode>
TileNodeRegistry::createTile(
    const TileKey& key,
    TerrainTileNode* parent,
    const TerrainSettings& settings,
    shared_ptr<TerrainTileHost> host)
{
    GeometryPool::Settings geomSettings {
        settings.tileSize,
        settings.skirtRatio,
        settings.morphTerrain
    };

    // Get a shared geometry from the pool that corresponds to this tile key:
    auto geometry = geometryPool->getPooledGeometry(
        key,
        geomSettings,
        nullptr);

    // initialize all the per-tile uniforms the shaders will need:
    float range, morphStart, morphEnd;
    selectionInfo->get(key, range, morphStart, morphEnd);

    float one_over_end_minus_start = 1.0f / (morphEnd - morphStart);
    fvec2 morphConstants = fvec2(morphEnd * one_over_end_minus_start, one_over_end_minus_start);

    unsigned numLODs = selectionInfo->getNumLODs();
    float childrenVisibilityRange = selectionInfo->getLOD(key.getLOD() + 1)._visibilityRange;

    // Make the new terrain tile
    auto tile = TerrainTileNode::create(
        key,
        parent,
        geometry,
        morphConstants,
        childrenVisibilityRange,
        numLODs,
        stateFactory->defaultTileDescriptors,
        host);

    //tile->setDoNotExpire(key.getLOD() == terrain.firstLOD);

    // set parent?
    // tile->setParent(parent) ... ?

    // Generate its state group:
    stateFactory->refreshStateGroup(tile);

    return tile;
}
#endif

void
TileNodeRegistry::createTileChildren(
    TerrainTileNode* parent,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    // make sure we're not already working on it
    if (parent->childLoader.isWorking() || parent->childLoader.isAvailable())
    {
        return;
    }

    // prepare variables to send to the async loader
    TileKey parent_key(parent->key);
    vsg::ref_ptr<TerrainTileNode> parent_weakptr(parent);

    // function that will create all 4 children and compile them
    auto create_children = [terrain, parent_key, parent_weakptr](Cancelable* io)
    {
        vsg::ref_ptr<vsg::Node> result;
        vsg::ref_ptr<TerrainTileNode> parent = parent_weakptr;
        if (parent)
        {
            auto quad = vsg::Group::create(); // TerrainTileQuad::create(parent_key, this);

            for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
            {
                if (io && io->isCanceled())
                    return result;

                TileKey childkey = parent_key.createChildKey(quadrant);

                auto tile = terrain->tiles->createTile(
                    childkey,
                    parent,
                    terrain);

                if (tile)
                {
                    quad->addChild(tile);
                }
            }

            result = quad;
        }

        // register me.
        //terrain.tiles.add(this, terrain);

        return result;
    };

    // queue up the job.
    parent->childLoader = terrain->runtime.compileAndAddNode(
        parent,
        create_children);
}

void
TileNodeRegistry::requestTileData(
    TerrainTileNode* tile,
    shared_ptr<TerrainContext> terrain) const
{
    //TODO.
}
