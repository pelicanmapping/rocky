/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileNodeRegistry.h"
#include "TerrainSettings.h"
#include "TerrainContext.h"
#include "RuntimeContext.h"

#include <rocky/TerrainTileModelFactory.h>

#include <vsg/ui/FrameStamp.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TileNodeRegistry] "

//----------------------------------------------------------------------------

TileNodeRegistry::TileNodeRegistry(TerrainTileHost* in_host) :
    _host(in_host)
{
    //nop
}

TileNodeRegistry::~TileNodeRegistry()
{
    releaseAll();
}

void
TileNodeRegistry::setDirty(
    const GeoExtent& extent,
    unsigned minLevel,
    unsigned maxLevel,
    const CreateTileManifest& manifest,
    shared_ptr<TerrainContext> terrain)
{
    std::scoped_lock lock(_mutex);
    
    for(auto& [key, entry] : _tiles)
    {
        if (minLevel <= key.levelOfDetail() && 
            maxLevel >= key.levelOfDetail() &&
            (!extent.valid() || extent.intersects(key.extent())))
        {
            entry._tile->refreshLayers(manifest);
        }
    }
}

void
TileNodeRegistry::releaseAll()
{
    std::scoped_lock lock(_mutex);

    _tiles.clear();
    _tracker.reset();
    _needsChildren.clear();
    _needsLoad.clear();
    _needsMerge.clear();
    _needsUpdate.clear();
}

void
TileNodeRegistry::ping(
    TerrainTileNode* tile0,
    TerrainTileNode* tile1,
    TerrainTileNode* tile2,
    TerrainTileNode* tile3,
    vsg::RecordTraversal& nv)
{
    std::scoped_lock lock(_mutex);

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
                entry._trackerToken = _tracker.use(tile, nullptr);
            }
            else
            {
                _tracker.use(tile, i->second._trackerToken);
            }

            // "progressive" means do not load LOD N+1 until LOD N is complete.
            const bool progressive = true;

            if (progressive)
            {
                auto parent = tile->parentTile();
                auto tile_complete = tile->dataMerger.available();

                if (tile->_needsChildren && tile_complete)
                    _needsChildren.push_back(tile->key);

                if (tile->dataLoader.idle() && (!parent || parent->dataMerger.available()))
                    _needsLoad.push_back(tile->key);
            }
            else
            {
                // free for all
                if (tile->_needsChildren)
                    _needsChildren.push_back(tile->key);

                if (tile->dataLoader.idle())
                    _needsLoad.push_back(tile->key);
            }

            // This will only queue one merge per frame, to prevent overloading
            // the (synchronous) update cycle in VSG.
            if (tile->dataLoader.available() && tile->dataMerger.idle())
                _needsMerge.push_back(tile->key);

            if (tile->_needsUpdate)
                _needsUpdate.push_back(tile->key);
        }
    }
}

void
TileNodeRegistry::update(
    const vsg::FrameStamp* fs,
    const IOOptions& io,
    shared_ptr<TerrainContext> terrain)
{
    std::scoped_lock lock(_mutex);

    //Log::info()
    //    << "Frame " << fs->frameCount << ": "
    //    << "tiles=" << _tracker._list.size() << " "
    //    << "needsChildren=" << _needsChildren.size() << " "
    //    << "needsLoad=" << _needsLoad.size() << " "
    //    << "needsMerge=" << _needsMerge.size() << std::endl;

    // update any tiles that asked for it
    for (auto& key : _needsUpdate)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            iter->second._tile->update(fs, io);
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
    for (auto& key : _needsLoad)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoad(iter->second._tile.get(), io, terrain);
        }
    }
    _needsLoad.clear();

    // schedule any data merging requests
    for (auto& key : _needsMerge)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMerge(iter->second._tile.get(), io, terrain);
            break; // one per frame :)
        }
    }
    _needsMerge.clear();

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
            auto key = tile->key;
            auto parent_iter = _tiles.find(key.createParentKey());
            if (parent_iter != _tiles.end())
            {
                auto parent = parent_iter->second._tile;
                if (parent.valid())
                    parent->unloadChildren();
            }
            _tiles.erase(key);
            return true;
        }
        return false;
    };

    _tracker.flush(~0, dispose);
}

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

    // Calculate the visibility range for this tile's children.
    float childrenVisibilityRange = FLT_MAX;
    if (key.levelOfDetail() < (terrain->selectionInfo->getNumLODs() - 1))
    {
        auto[tw, th] = key.profile().numTiles(key.levelOfDetail());
        TileKey testKey = key.createChildKey((key.tileY() <= th / 2) ? 0 : 3);
        childrenVisibilityRange = terrain->selectionInfo->getRange(testKey);
    }

    // Make the new terrain tile
    auto tile = TerrainTileNode::create(
        key,
        parent,
        geometry,
        morphConstants,
        childrenVisibilityRange,
        terrain->worldSRS,
        terrain->stateFactory->defaultTileDescriptors,
        terrain->tiles->_host,
        terrain->runtime);

    // Generate its state group:
    terrain->stateFactory->updateTerrainTileDescriptors(
        tile->renderModel,
        tile->stategroup,
        terrain->runtime);

    return tile;
}

vsg::ref_ptr<TerrainTileNode>
TileNodeRegistry::getTile(const TileKey& key) const
{
    std::scoped_lock lock(_mutex);
    auto iter = _tiles.find(key);
    return
        iter != _tiles.end() ? iter->second._tile :
        vsg::ref_ptr<TerrainTileNode>(nullptr);
}

void
TileNodeRegistry::createTileChildren(
    TerrainTileNode* parent,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    // make sure we're not already working on it
    if (parent->childLoader.working() || parent->childLoader.available())
    {
        return;
    }

    // prepare variables to send to the async loader
    TileKey parent_key(parent->key);
    vsg::ref_ptr<TerrainTileNode> parent_weakptr(parent);

    // function that will create all 4 children and compile them
    auto create_children = [terrain, parent_key, parent_weakptr](Cancelable& p)
    {
        vsg::ref_ptr<vsg::Node> result;
        vsg::ref_ptr<TerrainTileNode> parent = parent_weakptr;
        if (parent)
        {
            auto quad = vsg::Group::create();

            for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
            {
                if (p.canceled())
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

        return result;
    };

    // queue up the job.
    parent->childLoader = terrain->runtime.compileAndAddNode(
        parent,
        create_children);
}

namespace
{
    template<class T>
    struct PromiseOperation : public vsg::Operation
    {
        util::Promise<T> _promise;
        std::function<T(Cancelable&)> _func;

        PromiseOperation(std::function<T(Cancelable&)> func) :
            _func(func) { }

        void run() override {
            if (!_promise.abandoned())
                _promise.resolve(_func(_promise));
            else
                _promise.resolve();
        }

        util::Future<T> future() {
            return _promise.future();
        }
    };
}

void
TileNodeRegistry::requestLoad(
    TerrainTileNode* tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataLoader.working() || tile->dataLoader.available())
    {
        return;
    }

    auto key = tile->key;
    CreateTileManifest manifest;
    const IOOptions io(in_io);

    auto load = [key, manifest, terrain, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            return { };
        }

        TerrainTileModelFactory factory;

        auto model = factory.createTileModel(
            terrain->map.get(),
            key,
            manifest,
            IOOptions(io, p));

        return model;
    };

#if 1
    // a callback that will return the loading priority of a tile
    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile(tile_weak);
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    tile->dataLoader = util::job::dispatch<TerrainTileModel>(
       load, {
            "dataLoader",
            priority_func,
            util::job_scheduler::get(terrain->loadSchedulerName),
            nullptr
        }
    );

#else
    auto load_op = vsg::ref_ptr<PromiseOperation<TerrainTileModel>>(
        new PromiseOperation<TerrainTileModel>(load));

    tile->dataLoader = load_op->future();

    terrain->runtime.loaders->add(load_op);
#endif
}

void
TileNodeRegistry::requestMerge(
    TerrainTileNode* tile,
    const IOOptions& in_io,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataMerger.working() || tile->dataMerger.available())
    {
        return;
    }

    auto key = tile->key;
    const IOOptions io(in_io);

    auto merge = [key, terrain](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            return false;
        }

        //util::scoped_chrono timer("merge sync " + key.str());

        auto tile = terrain->tiles->getTile(key);
        if (tile)
        {
            auto model = tile->dataLoader.get();

            auto& renderModel = tile->renderModel;
            auto compiler = terrain->runtime.compiler();

            if (model.colorLayers.size() > 0)
            {
                auto& layer = model.colorLayers[0];
                if (layer.image.valid())
                {
                    renderModel.color.image = layer.image.image();
                    renderModel.color.matrix = layer.matrix;
                }
            }

            if (model.elevation.heightfield.valid())
            {
                renderModel.elevation.image = model.elevation.heightfield.heightfield();
                renderModel.elevation.matrix = model.elevation.matrix;

                // prompt the tile can update its bounds
                tile->setElevation(
                    renderModel.elevation.image,
                    renderModel.elevation.matrix);
            }

            if (model.normalMap.image.valid())
            {
                renderModel.normal.image = model.normalMap.image.image();
                renderModel.normal.matrix = model.normalMap.matrix;
            }

            terrain->stateFactory->updateTerrainTileDescriptors(
                renderModel,
                tile->stategroup,
                terrain->runtime);
        }

        return true;
    };

    auto merge_op = vsg::ref_ptr<PromiseOperation<bool>>(
        new PromiseOperation<bool>(merge));

    tile->dataMerger = merge_op->future();

    terrain->runtime.updates()->add(merge_op);
}
