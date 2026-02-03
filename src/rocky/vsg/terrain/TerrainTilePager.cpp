/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTilePager.h"
#include "TerrainEngine.h"
#include "TerrainSettings.h"
#include "TerrainTileNode.h"
#include "TerrainTileHost.h"
#include "SurfaceNode.h"
#include "../VSGUtils.h"
#include <rocky/TerrainTileModelFactory.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TerrainTilePager] "

#define RP_DEBUG if(false) Log()->info

//----------------------------------------------------------------------------

TerrainTilePager::TerrainTilePager(const TerrainSettings& settings, TerrainTileHost* host) :
    _host(host),
    _settings(settings)
{
    _firstLOD = settings.minLevel;
}

TerrainTilePager::~TerrainTilePager()
{
    releaseAll();
}

void
TerrainTilePager::releaseAll()
{
    std::scoped_lock lock(_mutex);

    _tiles.clear();
    _tracker.reset();
    _createChildren.clear();
    _loadData.clear();
    _mergeData.clear();
    _updateData.clear();
}

void
TerrainTilePager::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& rv)
{
    if (_settings.supportMultiThreadedRecord)
        _mutex.lock();

    // first, update the tracker to keep this tile alive.
    auto& info = _tiles[tile->key];
    if (!info.tile)
        info.tile = tile;

    if (info.trackerToken)
        info.trackerToken = _tracker.update(info.trackerToken);
    else
        info.trackerToken = _tracker.emplace(info.tile);

    // next, see if the tile needs anything.
    // "progressive" means do not load LOD N+1 until LOD N is complete.
    // This is currently the only option.
    const bool progressive = true;
    if (progressive)
    {
        // If this tile is fully merged, and it needs children, queue them up to load.
        if (info.dataMerger.available() && tile->needsSubtiles)
        {
            _createChildren.push_back(tile->key);
        }

        if (parent == nullptr)
        {
            // If this is a root tile, and it needs data, queue that up:
            if (info.dataLoader.empty())
            {
                _loadData.emplace_back(tile->key);
            }
        }
        else
        {
            // If this is a non-root tile that needs data, check to make sure the 
            // parent's tile is done loaded before queueing that up.
            auto& parent_info = _tiles[parent->key];
            if (!parent_info.tile)
            {
                ROCKY_SOFT_ASSERT_AND_RETURN(parent_info.tile, void());
            }
            if (parent_info.tile && parent_info.dataMerger.available() && info.dataLoader.empty())
            {
                _loadData.push_back(tile->key);
            }
        }
    }

    // If a data-load is complete and ready to merge, queue it up.
    // This will only queue one merge per frame to prevent overloading
    // the (synchronous) update cycle in VSG.
    if (info.dataLoader.available() && info.dataMerger.empty())
    {
        _mergeData.push_back(tile->key);
    }

    // Tile updates are TBD.
    if (tile->needsUpdate)
    {
        _updateData.push_back(tile->key);
    }

    if (_settings.supportMultiThreadedRecord)
        _mutex.unlock();
}

bool
TerrainTilePager::update(const vsg::FrameStamp* fs, const IOOptions& io, std::shared_ptr<TerrainEngine> engine)
{
    std::scoped_lock lock(_mutex);

    bool changes = false;

    changes =
        !_updateData.empty() ||
        !_createChildren.empty() ||
        !_loadData.empty() ||
        !_mergeData.empty();

    //Log::info()
    //    << "Frame " << fs->frameCount << ": "
    //    << "tiles=" << _tracker._list.size()-1 << " "
    //    << "needsSubtiles=" << _loadSubtiles.size() << " "
    //    << "needsLoad=" << _loadData.size() << " "
    //    << "needsMerge=" << _mergeData.size() << std::endl;

    // update any tiles that asked for it
    for (auto& key : _updateData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            if (iter->second.tile->update(fs, io))
                changes = true;
        }
    }
    _updateData.clear();

    // launch any "new subtiles" requests
    for (auto& key : _createChildren)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestCreateChildren(iter->second, engine); // parent, context
            iter->second.tile->needsSubtiles = false;
        }

        changes = true;
    }
    _createChildren.clear();

    // launch any data loading requests
    for (auto& key : _loadData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadData(iter->second, io, engine);
        }

        changes = true;
    }
    _loadData.clear();

    // schedule any data merging requests
    for (auto& key : _mergeData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMergeData(iter->second, io, engine);
        }

        changes = true;
    }
    _mergeData.clear();

    // Flush unused tiles (i.e., tiles that failed to ping) out of the system.
    // Tiles ping their children all at once; this should in theory prevent
    // a child from expiring without its siblings.
    // Only do this is the frame advanced - otherwise just leave it be.
    if (fs->frameCount > _lastUpdate)
    {
        const auto dispose = [&](TerrainTileNode* tile)
        {
            if (!tile->doNotExpire)
            {
                auto key = tile->key;
                auto parent_iter = _tiles.find(key.createParentKey());
                if (parent_iter != _tiles.end())
                {
                    auto parent = parent_iter->second.tile;
                    if (parent.valid())
                    {
                        // Feed the children to the garbage disposal before removing them
                        // so any vulkan objects are safely destroyed
                        if (tile->children.size() > 1)
                            engine->context->dispose(tile->children[1]);

                        tile->children.resize(1);
                        tile->subtilesLoader.reset();
                        tile->needsSubtiles = false;
                    }
                }
                _tiles.erase(key);
                return true;
            }
            return false;
        };

        _tracker.flush(~0, _settings.tileCacheSize.value_or(0u), dispose);
    }

    // synchronize
    _lastUpdate = fs->frameCount;

    return changes;
}

vsg::ref_ptr<TerrainTileNode>
TerrainTilePager::getTile(const TileKey& key) const
{
    std::scoped_lock lock(_mutex);
    auto iter = _tiles.find(key);
    return
        iter != _tiles.end() ? iter->second.tile :
        vsg::ref_ptr<TerrainTileNode>(nullptr);
}

void
TerrainTilePager::requestCreateChildren(TileInfo& info, std::shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile && engine, void());

    // make sure we're not already working on it
    if (!info.childrenCreator.empty())
        return;

    RP_DEBUG("requestLoadSubtiles -> {}", info.tile->key.str());

    vsg::observer_ptr<TerrainTileNode> weak_parent(info.tile);

    // function that will create all 4 children and compile them
    auto create_children = [engine, weak_parent](Cancelable& p)
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

                auto tile = engine->createTile(childkey, parent);

                ROCKY_SOFT_ASSERT_AND_RETURN(tile != nullptr, result);

                quad->children[quadrant] = tile;
            }

            // assign the result once all 4 children are added
            result = quad;
        }

        if (result)
        {
            engine->context->compile(result);

            engine->context->onNextUpdate([result, engine, weak_parent]()
                {
                    auto parent = weak_parent.ref_ptr();
                    if (parent)
                    {
                        parent->addChild(result);
                    }
                    engine->context->requestFrame();
                });

            engine->context->requestFrame();
        }

        return result;
    };

    // a callback that will return the loading priority of a tile
    auto priority_func = [weak_parent]() -> float
    {
        auto tile = weak_parent.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.level) : -FLT_MAX;
    };

    info.childrenCreator = jobs::dispatch(
        create_children,
        jobs::context {
            "create child " + info.tile->key.str(),
            jobs::get_pool(engine->loadSchedulerName),
            priority_func,
            nullptr
        });
}

void
TerrainTilePager::requestLoadData(TileInfo& info, const IOOptions& in_io, std::shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile, void());

    // make sure we're not already working on it
    if (info.dataLoader.working() || info.dataLoader.available())
    {
        return;
    }

    auto key = info.tile->key;
    auto tile = info.tile;

    RP_DEBUG("requestLoadData -> {}", key.str());

    const IOOptions io(in_io);

    auto load = [key, tile, engine, io](Cancelable& p) -> bool
    {
        if (p.canceled())
            return false;

        TerrainTileModelFactory factory;
        factory.compositeColorLayers = true;

        auto dataModel = factory.createTileModel(engine->map.get(), key, io.with(p));

        if (!dataModel.empty())
        {
            auto newRenderModel = engine->stateFactory.updateRenderModel(
                tile->renderModel,
                dataModel,
                engine->context);

            tile->renderModel = newRenderModel;

            engine->context->requestFrame();

            return true;
        }

        return false;
    };

    // a callback that will return the loading priority of a tile
    // we must use a WEAK pointer to allow job cancelation to work
    vsg::observer_ptr<TerrainTileNode> tile_weak(info.tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.level) : -FLT_MAX;
    };

    info.dataLoader = jobs::dispatch(
        load, 
        jobs::context {
            "load data " + key.str(),
            jobs::get_pool(engine->loadSchedulerName),
            priority_func,
            nullptr
        } );
}

void
TerrainTilePager::requestMergeData(TileInfo& info, const IOOptions& in_io, std::shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile, void());

    // make sure we're not already working on it
    if (info.dataMerger.working() || info.dataMerger.available())
    {
        return;
    }

    auto key = info.tile->key;

    // if the loader didn't load anything, we're done.
    if (info.dataLoader.value() == false)
    {
        info.dataMerger.resolve(true);
        return;
    }

    // operation to dispose of the old state command and replace it with a new one:
    auto merge = [key, engine](Cancelable& c)
    {
        auto tile = engine->host->tiles().getTile(key);
        if (tile)
        {
            for (auto c : tile->stategroup->stateCommands)
                engine->context->dispose(c);

            tile->stategroup->stateCommands = { tile->renderModel.descriptors.bind };

            tile->surface->setElevation(
                tile->renderModel.elevation.image,
                tile->renderModel.elevation.matrix);

            engine->context->requestFrame();
            return true;
        }
        return false;
    };

    auto merge_operation = util::PromiseOperation<bool>::create(merge);
    info.dataMerger = merge_operation->future();

    vsg::observer_ptr<TerrainTileNode> weak_tile(info.tile);
    auto priority_func = [weak_tile]() -> float
    {
        auto tile = weak_tile.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.level) : -FLT_MAX;
    };

    engine->context->onNextUpdate(merge_operation, priority_func);
}
