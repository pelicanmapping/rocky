/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

#include "LoadTileData.h"
#include "TerrainTileNode.h"

#include <rocky/Notify.h>
#include <rocky/TerrainTileModelFactory.h>

using namespace rocky;


LoadTileDataOperation::LoadTileDataOperation(
    shared_ptr<Map> map,
    TerrainTileNode* tilenode) :

    _map(map),
    _tilenode(tilenode),
    _enableCancel(true),
    _dispatched(false),
    _merged(false)
{
    _name = tilenode->key.str();
}

LoadTileDataOperation::LoadTileDataOperation(
    shared_ptr<Map> map,
    const CreateTileManifest& manifest, 
    TerrainTileNode* tilenode) :

    _map(map),
    _manifest(manifest),
    _tilenode(tilenode),
    _enableCancel(true),
    _dispatched(false),
    _merged(false)
{
    _name = tilenode->key.str();
}

LoadTileDataOperation::~LoadTileDataOperation()
{
    //if (!_dispatched || !_merged)
    //{
    //    OE_INFO << _name << " dispatched=" << _dispatched << " merged=" << _merged << std::endl;
    //}
}

bool
LoadTileDataOperation::dispatch(bool async)
{
    vsg::ref_ptr<TerrainTileNode> tile(_tilenode);
    if (!tile) return false;

    _dispatched = true;

    CreateTileManifest manifest(_manifest);
    bool enableCancel = _enableCancel;

    TileKey key(tile->key);

    shared_ptr<Map> map = _map.lock();

    auto load = [map, key, manifest, enableCancel] (IOControl* progress)
    {
        TerrainTileModelFactory factory;

        return factory.createTileModel(
            map.get(),
            key,
            manifest,
            progress);
    };

    // Priority function. This return the maximum priority if the tile
    // has disappeared so that it will be immediately rejected from the job queue.
    // You can change it to -FLT_MAX to let it fester on the end of the queue,
    // but that may slow down the job queue's sorting algorithm.
    vsg::observer_ptr<TerrainTileNode> tile_obs(_tilenode);
    auto priority_func = [tile_obs]() -> float
    {
        if (tile_obs.valid() == false) return FLT_MAX; // quick trivial reject
        vsg::ref_ptr<TerrainTileNode> tilenode(tile_obs);
        return FLT_MAX; // tilenode ? tilenode->getLoadPriority() : FLT_MAX;
    };


    if (async)
    {
        util::Job job;
        //job.setArena(ARENA_LOAD_TILE);
        job.setPriorityFunction(priority_func);
        _result = job.dispatch<TerrainTileModel>(load);
    }
    else
    {
        util::Promise<TerrainTileModel> promise;
        _result = promise.getFuture();
        promise.resolve(load(nullptr));
    }

    return true;
}


bool
LoadTileDataOperation::merge(shared_ptr<TerrainContext> terrain)
{
    _merged = true;

    shared_ptr<Map> map = _map.lock();
    if (!map) return false;

    vsg::ref_ptr<TerrainTileNode> tile(_tilenode);
    if (!tile) return false;

    // no data model at all - done
    // GW: should never happen.
    if (!_result.isAvailable())
    {
        ROCKY_WARN << tile->key.str() << " bailing out of merge b/c data model is NULL" << std::endl;
        return false;
    }

    ROCKY_SOFT_ASSERT_AND_RETURN(_result.isAvailable(), false);

    auto& model = _result.get();

    // Check the map data revision and scan the manifest and see if any
    // revisions don't match the revisions in the original manifest.
    // If there are mismatches, that means the map has changed since we
    // submitted this request, and the results are now invalid.
    if (model.revision != map->getDataModelRevision() ||
        _manifest.inSyncWith(map.get()) == false)
    {
        // wipe the data model, update the revisions, and try again.
        //ROCKY_DEBUG << LC << "Request for tile " << tilenode->getKey().str() << " out of date and will be requeued" << std::endl;
        _manifest.updateRevisions(map.get());
        tile->refreshLayers(_manifest);
        return false;
    }

    // Merge the new data into the tile.
    tile->merge(model, _manifest, terrain);

    return true;
}
