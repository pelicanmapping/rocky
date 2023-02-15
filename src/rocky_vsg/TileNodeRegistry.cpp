/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileNodeRegistry.h"
#include "TerrainSettings.h"
#include "TerrainContext.h"
#include "RuntimeContext.h"
#include "Utils.h"

#include <rocky/TerrainTileModelFactory.h>

#include <vsg/nodes/QuadGroup.h>
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
    _loadChildren.clear();
    _loadData.clear();
    _mergeData.clear();
    _updateData.clear();
}

void
TileNodeRegistry::ping(TerrainTileNode* tile, bool parentHasData, vsg::RecordTraversal& rv)
{
    // first, update the tracker to keep this tile alive.
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

    // next, see if the tile needs anything.
    // 
    // "progressive" means do not load LOD N+1 until LOD N is complete.
    const bool progressive = true;

    if (progressive)
    {
        auto tileHasData = tile->dataMerger.available();

        if (tileHasData && tile->_needsChildren)
            _loadChildren.push_back(tile->key);

        if (parentHasData && tile->dataLoader.idle())
            _loadData.push_back(tile->key);
    }
    else
    {
        //// free for all
        //if (tile->_needsChildren)
        //    _needsChildren.push_back(tile->key);

        //if (tile->dataLoader.idle())
        //    _needsLoad.push_back(tile->key);
    }

    // This will only queue one merge per frame, to prevent overloading
    // the (synchronous) update cycle in VSG.
    if (tile->dataLoader.available() && tile->dataMerger.idle())
        _mergeData.push_back(tile->key);

    if (tile->_needsUpdate)
        _updateData.push_back(tile->key);
}

#if 0
void
TileNodeRegistry::ping(TerrainTileNode* parent, vsg::QuadGroup* quad, vsg::RecordTraversal& nv)
{
    std::scoped_lock lock(_mutex);

    if (quad == nullptr && parent != nullptr)
    {
        // special case - ping the parent.
        ping(nullptr, parent);
    }
    else if (quad != nullptr)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            TerrainTileNode* tile = static_cast<TerrainTileNode*>(quad->children[i].get());
            if (tile)
            {
                ping(parent, tile);
            }
        }
    }
}
#endif

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
    //    << "needsChildren=" << _loadChildren.size() << " "
    //    << "needsLoad=" << _loadData.size() << " "
    //    << "needsMerge=" << _mergeData.size() << std::endl;

    // update any tiles that asked for it
    for (auto& key : _updateData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            iter->second._tile->update(fs, io);
        }
    }
    _updateData.clear();

    // launch any "new children" requests
    for (auto& key : _loadChildren)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadChildren(
                iter->second._tile, // parent
                terrain);  // context

            iter->second._tile->_needsChildren = false;
        }
    }
    _loadChildren.clear();

    // launch any data loading requests
    for (auto& key : _loadData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadData(iter->second._tile, io, terrain);
        }
    }
    _loadData.clear();

    // schedule any data merging requests
    for (auto& key : _mergeData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMergeData(iter->second._tile, io, terrain);
            break; // one per frame :)
        }
    }
    _mergeData.clear();

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
    vsg::ref_ptr<TerrainTileNode> parent,
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

    // inherit model data from the parent
    if (parent)
        tile->inheritFrom(parent);

    // update the bounding sphere for culling
    tile->recomputeBound();

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
TileNodeRegistry::requestLoadChildren(
    vsg::ref_ptr<TerrainTileNode> parent,
    shared_ptr<TerrainContext> terrain) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    // make sure we're not already working on it
    if (!parent->childrenLoader.idle())
        return;

    vsg::observer_ptr<TerrainTileNode> weak_parent(parent);

    // function that will create all 4 children and compile them
    auto create_children = [terrain, weak_parent](Cancelable& p)
    {
        vsg::ref_ptr<vsg::Node> result;
        auto parent = weak_parent.ref_ptr();
        if (parent)
        {
            auto quad = vsg::QuadGroup::create();

            for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
            {
                if (p.canceled())
                    return result;

                TileKey childkey = parent->key.createChildKey(quadrant);

                auto tile = terrain->tiles->createTile(
                    childkey,
                    parent,
                    terrain);

                ROCKY_SOFT_ASSERT_AND_RETURN(tile != nullptr, result);

                quad->children[quadrant] = tile;
            }

            // assign the result once all 4 children are added
            result = quad;
        }

        return result;
    };

    // a callback that will return the loading priority of a tile
    auto priority_func = [weak_parent]() -> float
    {
        auto tile = weak_parent.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    parent->childrenLoader = terrain->runtime.compileAndAddChild(
        parent,
        create_children,
        {
            "create child " + parent->key.str(),
            priority_func,
            util::job_scheduler::get(terrain->loadSchedulerName),
            nullptr
        });
}

void
TileNodeRegistry::requestLoadData(
    vsg::ref_ptr<TerrainTileNode> tile,
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

    // a callback that will return the loading priority of a tile
    // we must use a WEAK pointer to allow job cancelation to work
    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    tile->dataLoader = util::job::dispatch<TerrainTileModel>(
       load, {
            "load data " + key.str(),
            priority_func,
            util::job_scheduler::get(terrain->loadSchedulerName),
            nullptr
        }
    );
}

void
TileNodeRegistry::requestMergeData(
    vsg::ref_ptr<TerrainTileNode> tile,
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

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    tile->dataMerger = merge_op->future();

    terrain->runtime.updates()->add(merge_op);
}
