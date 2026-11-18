/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "TerrainTilePager.h"
#include "TerrainTileFactory.h"
#include "TerrainSettings.h"
#include "TerrainTileNode.h"
#include "TerrainTileHost.h"
#include "SurfaceNode.h"
#include "../VSGUtils.h"
#include <rocky/TerrainTileModelFactory.h>
#include <rocky/Map.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define LC "[TerrainTilePager] "

#define RP_DEBUG if(false) Log()->info

//----------------------------------------------------------------------------

TerrainTilePager::TerrainTilePager(const TerrainSettings& settings, TerrainTileHost* host, VSGContext vsgcontext) :
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
    _vectorLoads.clear();
    _vectorDemand.clear();
}

void
TerrainTilePager::recompileDescriptorSets(VSGContext vsgcontext)
{
    //nop
}

void
TerrainTilePager::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& rv)
{
    ScopedPredicateLock lock(_mutex, _settings.supportMultiThreadedRecord);

    // first, update the tracker to keep this tile alive.
    auto& info = _tiles[tile->key];
    if (!info.tile)
        info.tile = tile;

    if (info.trackerToken)
        info.trackerToken = _tracker.update(info.trackerToken);
    else
        info.trackerToken = _tracker.emplace(info.tile);

    // Keep the last recorded demand through idle updates in render-on-demand applications.
    // Only a new record frame replaces it; an idle update is not a visibility change.
    if (_vectorDemandFrame != rv.getFrameStamp()->frameCount)
    {
        _vectorDemand.clear();
        _vectorDemandFrame = rv.getFrameStamp()->frameCount;
    }
    // Sibling pings preserve terrain residency, but only drawn surfaces request vector work.
    if (tile->lastSurfaceFrame.load() == rv.getFrameStamp()->frameCount &&
        rv.traversalMask != VSG_SHADOW_CAMERA_TRAVERSAL_MASK)
        _vectorDemand.push_back(tile->key);

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
            auto parent_iter = _tiles.find(parent->key);
            if (parent_iter == _tiles.end())
            {
                // Parent was evicted from the tile cache but the child
                // still references it in the scene graph. This is benign;
                // the parent will be re-registered on its next ping.
            }
            else if (parent_iter->second.dataMerger.available() && info.dataLoader.empty())
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
        _mergeData.emplace_back(tile->key);
    }

    // Tile updates are TBD.
    if (tile->needsUpdate)
    {
        _updateData.emplace_back(tile->key);
    }
}

bool
TerrainTilePager::update(std::shared_ptr<TerrainTileFactory> tileFactory, VSGContext vsgcontext)
{
    std::scoped_lock lock(_mutex);

    auto fs = vsgcontext->viewer()->getFrameStamp();
    auto& io = vsgcontext->io;

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
            requestCreateChildren(iter->second, tileFactory, vsgcontext); // parent, context
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
            requestLoadData(iter->second, io, tileFactory, vsgcontext);
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
            requestMergeData(iter->second, io, tileFactory, vsgcontext);
        }

        changes = true;
    }
    _mergeData.clear();

    changes = updateVectors(tileFactory, vsgcontext) || changes;

    // Flush unused tiles (i.e., tiles that failed to ping) out of the system.
    // Tiles ping their children all at once; this should in theory prevent
    // a child from expiring without its siblings.
    // Only do this is the frame advanced - otherwise just leave it be.
    if (fs->frameCount > _lastUpdate)
    {
        const auto dispose = [&, vsgcontext](TerrainTileNode* tile)
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
                            vsgcontext->dispose(tile->children[1]);

                        tile->children.resize(1);
                        tile->subtilesLoader.reset();
                        tile->needsSubtiles = false;
                        _host->activity().onTileBoundsChanged.fire(tile->key);
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
TerrainTilePager::requestCreateChildren(TileInfo& info, std::shared_ptr<TerrainTileFactory> tileFactory, VSGContext vsgcontext) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile && tileFactory, void());

    // make sure we're not already working on it
    if (!info.childrenCreator.empty())
        return;

    RP_DEBUG("requestLoadSubtiles -> {}", info.tile->key.str());

    std::weak_ptr<TerrainTileFactory> weak_tileFactory(tileFactory);
    vsg::observer_ptr<TerrainTileNode> weak_parent(info.tile);

    // function that will create all 4 children and compile them
    auto create_children = [weak_tileFactory, weak_parent, vsgcontext](Cancelable& p) -> vsg::ref_ptr<vsg::Node>
    {
        vsg::ref_ptr<vsg::Node> result;

        auto tileFactory = weak_tileFactory.lock();
        if (!tileFactory) return result;

        auto parent = weak_parent.ref_ptr();
        if (!parent) return result;

        auto quad = vsg::QuadGroup::create();

        for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
        {
            if (p.canceled())
                return result;

            TileKey childkey = parent->key.createChildKey(quadrant);

            auto tile = tileFactory->createTile(childkey, parent, vsgcontext);

            if (!tile)
            {
                vsgcontext->dispose(quad);
                return {};
            }

            quad->children[quadrant] = tile;
        }

        // assign the result once all 4 children are added
        result = quad;

        if (result)
        {
            auto cr = vsgcontext->compile(result);
            if (cr.result != VK_SUCCESS)
            {
                Log()->warn("TerrainTilePager: children of {} failed to compile ({}): {}",
                    parent->key.str(), cr.result, cr.message);
                vsgcontext->dispose(result);
                return {};
            }

            vsgcontext->onNextUpdate([result, weak_parent, weak_tileFactory](VSGContext vsgcontext)
                {
                    if (auto parent = weak_parent.ref_ptr())
                    {
                        parent->addChild(result);
                        if (auto factory = weak_tileFactory.lock())
                            factory->host->activity().onTileBoundsChanged.fire(parent->key);
                        vsgcontext->requestFrame();
                    }
                });

            vsgcontext->requestFrame();
        }

        return result;
    };

    // a callback that will return the loading priority of a tile
    auto priority_func = [weak_parent]() -> float
    {
        auto tile = weak_parent.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.level) : -FLT_MAX;
    };

    auto& jobs = vsgcontext->io.services().jobs;

    info.childrenCreator = jobs.dispatch(
        create_children,
        jobs::context {
            "create child " + info.tile->key.str(),
            jobs.get_pool(tileFactory->loadSchedulerName),
            priority_func,
            nullptr
        });
}

void
TerrainTilePager::requestLoadData(TileInfo& info, const IOOptions& in_io,
    std::shared_ptr<TerrainTileFactory> tileFactory, VSGContext vsgcontext) const
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

    auto load = [key, tile, tileFactory, io, vsgcontext](Cancelable& p) -> bool
    {
        if (p.canceled())
            return false;

        TerrainTileModelFactory factory;
        factory.compositeColorLayers = true;

        auto dataModel = factory.createTileModel(tileFactory->map.get(), key, io.with(p));

        if (!dataModel.empty())
        {
            std::scoped_lock lock(tile->renderModelMutex);
            auto newRenderModel = tileFactory->state->tryUpdateRenderModel(tile->key, tile->renderModel, dataModel, vsgcontext);

            if (newRenderModel.failed())
                return false;

            tile->renderModel = newRenderModel.release();

            vsgcontext->requestFrame();

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

    auto& jobs = vsgcontext->io.services().jobs;

    info.dataLoader = jobs.dispatch(
        load, 
        jobs::context {
            "load data " + key.str(),
            jobs.get_pool(tileFactory->loadSchedulerName),
            priority_func,
            nullptr
        } );
}

void
TerrainTilePager::requestMergeData(TileInfo& info, const IOOptions& in_io,
    std::shared_ptr<TerrainTileFactory> tileFactory, VSGContext vsgcontext) const
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
    std::weak_ptr<TerrainTileFactory> weak_tileFactory(tileFactory);

    auto merge = [key, weak_tileFactory, vsgcontext](Cancelable& c) -> bool
    {
        if (c.canceled()) return false;
        auto tileFactory = weak_tileFactory.lock();
        if (!tileFactory) return false;

        auto tile = tileFactory->host->tiles().getTile(key);
        if (tile)
        {
            // Serialize publication with worker-side base-data updates and parent inheritance.
            std::scoped_lock lock(tile->renderModelMutex);
            // get rid of old state:
            for (auto c : tile->stategroup->stateCommands)
                vsgcontext->dispose(c);

            // set new ones:
            tile->stategroup->stateCommands = {
                tile->renderModel.descriptors.bind
            };

            // update the surface proxy witth new elevation data:
            tile->surface->setElevation(
                tile->renderModel.elevation.image,
                tile->renderModel.elevation.matrix);

            // Keep the tile's LOD/culling sphere synchronized with the
            // elevation-aware bounds recomputed by SurfaceNode::setElevation().
            tile->bound = tile->surface->recomputeBound();

            // notify the activity interface
            tileFactory->host->activity().onTileLoaded.fire(key);
            tileFactory->host->activity().onTileBoundsChanged.fire(key);

            vsgcontext->requestFrame();
            return true;
        }
        return false;
    };

    auto merge_operation = PromiseOperation<bool>::create(merge);
    info.dataMerger = merge_operation->future();

    vsg::observer_ptr<TerrainTileNode> weak_tile(info.tile);
    auto priority_func = [weak_tile]() -> float
    {
        auto tile = weak_tile.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.level) : -FLT_MAX;
    };

    vsgcontext->scheduleMeteredUpdate(merge_operation, priority_func);
}

bool TerrainTilePager::updateVectors(std::shared_ptr<TerrainTileFactory> factory, VSGContext context)
{
#ifndef ROCKY_HAS_SLUGHORN
    _vectorDemand.clear();
    return false;
#else
    if (factory->map->layers<VectorTileLayer>().empty())
    {
        _vectorDemand.clear();
        return false;
    }
    const auto frame = context->viewer()->getFrameStamp()->frameCount;
    // A main view that stopped traversing this terrain must eventually release its demand.
    if (frame > _vectorDemandFrame + 1u) _vectorDemand.clear();
    std::sort(_vectorDemand.begin(), _vectorDemand.end());
    _vectorDemand.erase(std::unique(_vectorDemand.begin(), _vectorDemand.end()), _vectorDemand.end());
    auto& jobs = context->io.services().jobs;
    auto pool = jobs.get_pool(factory->loadSchedulerName);
    auto device = context->device();
    const auto limit = device ? device->getPhysicalDevice()->getProperties().limits.maxImageDimension2D : 0u;
    bool changed = false;

    // Reset stale tile publications before the metered queue can install obsolete artwork.
    // Cached, invisible tiles retain their last good resource but produce no new vector work.
    for (auto& entry : _tiles)
    {
        if (!std::binary_search(_vectorDemand.begin(), _vectorDemand.end(), entry.first))
        {
            if (!entry.second.vectorPreparation.empty()) entry.second.vectorPreparation.reset();
            if (!entry.second.vectorMerger.empty()) entry.second.vectorMerger.reset();
            entry.second.vectorRequest.reset();
        }
    }
    for (auto& entry : _vectorLoads) entry.second.priority->store(-FLT_MAX);

    for (const auto& key : _vectorDemand)
    {
        auto found = _tiles.find(key);
        if (found == _tiles.end()) continue;
        auto& info = found->second;
        auto tile = info.tile;
        const auto request = terrainVectorRequest(*factory->map, key, factory->vectorStyleZoom);
        const auto identity = std::make_pair(request.key, request.signature);
        if (info.vectorRequest != identity)
        {
            info.vectorPreparation.reset();
            info.vectorMerger.reset();
            info.failedVectorPublication.reset();
            info.vectorRequest = identity;
        }
        TerrainTileRenderModel current;
        {
            std::scoped_lock lock(tile->renderModelMutex);
            current = tile->renderModel;
        }
        if (request.sources.empty() && !current.vectors) continue;
        const bool upToDate = current.vectors && current.vectors->key == request.key &&
            current.vectors->signature == request.signature;
        auto cached = _vectorLoads.find(identity);
        if (cached != _vectorLoads.end()) cached->second.lastUsed = frame;
        if (upToDate)
        {
            if (!info.vectorPreparation.empty()) info.vectorPreparation.reset();
            if (!info.vectorMerger.empty()) info.vectorMerger.reset();
            continue;
        }
        if (info.failedVectorPublication == identity) continue;

        auto& load = _vectorLoads[identity];
        load.lastUsed = frame;
        const float priority = -(std::sqrt(tile->lastTraversalRange.load()) * std::max(1u, key.level));
        load.priority->store(std::max(load.priority->load(), priority));
        if (load.future.empty())
        {
            auto io = context->io;
            auto getPriority = [weak = std::weak_ptr<std::atomic<float>>(load.priority)]() {
                auto p = weak.lock(); return p ? p->load() : -FLT_MAX;
            };
            load.future = jobs.dispatch([request, io, limit, context](Cancelable& cancel)
                -> Result<vsg::ref_ptr<TerrainVectorTexture>>
                {
                    auto result = buildTerrainVectorTexture(request, io.with(cancel), limit);
                    context->requestFrame();
                    return result;
                }, jobs::context{ "terrain vectors " + request.key.str(), pool, getPriority, nullptr });
        }
        if (!load.future.available()) continue;
        const auto& result = load.future.value();
        if (result.failed())
        {
            if (!load.reportedFailure)
            {
                Log()->warn("Terrain vectors {}: {}", request.key.str(), result.error().message);
                load.reportedFailure = true;
            }
            continue;
        }

        // Compile a snapshot on a terrain worker, alongside imagery/elevation loading.
        // No scene mutation or lengthy Vulkan compilation takes place in updateVectors.
        vsg::observer_ptr<TerrainTileNode> weakTile(tile);
        auto getPriority = [weakTile]() {
            auto t = weakTile.ref_ptr();
            return t ? -(std::sqrt(t->lastTraversalRange.load()) * std::max(1u, t->key.level)) : -FLT_MAX;
        };
        if (info.vectorPreparation.empty())
        {
            auto resource = result.value();
            auto state = factory->state;
            info.vectorPreparation = jobs.dispatch([key, current, resource, state, context](Cancelable& cancel)
                -> std::shared_ptr<VectorPublication>
                {
                    if (cancel.canceled()) return {};
                    auto publication = std::make_shared<VectorPublication>();
                    publication->context = context;
                    publication->base = current.descriptors.bind;
                    auto updated = state->tryUpdateRenderModel(key, current, {}, context, resource);
                    if (updated.ok()) publication->model = updated.release();
                    else Log()->warn("Terrain vector publication {}: {}", key.str(), updated.error().message);
                    context->requestFrame();
                    return cancel.canceled() ? nullptr : publication;
                }, jobs::context{ "prepare terrain vectors " + key.str(), pool, getPriority, nullptr });
        }
        if (!info.vectorPreparation.available()) continue;
        auto publication = info.vectorPreparation.value();
        if (!publication || !publication->model)
        {
            info.failedVectorPublication = identity;
            info.vectorPreparation.reset();
            continue;
        }
        if (info.vectorMerger.available())
        {
            // An imagery update won the race. Rebase the vector descriptor snapshot on its latest model.
            if (!info.vectorMerger.value())
            {
                info.vectorPreparation.reset();
                info.vectorMerger.reset();
                context->requestFrame();
            }
            continue;
        }
        if (!info.vectorMerger.empty()) continue;
        std::weak_ptr<TerrainTileFactory> weakFactory(factory);
        auto merge = [weakTile, weakFactory, publication, context](Cancelable& cancel) -> bool
        {
            if (cancel.canceled()) return false;
            auto tile = weakTile.ref_ptr();
            auto factory = weakFactory.lock();
            if (!tile || !factory || factory->host->tiles().getTile(tile->key) != tile) return false;
            std::scoped_lock lock(tile->renderModelMutex);
            if (tile->renderModel.descriptors.bind != publication->base) return false;
            for (auto& command : tile->stategroup->stateCommands) context->dispose(command);
            tile->renderModel = *publication->model;
            tile->stategroup->stateCommands = { tile->renderModel.descriptors.bind };
            tile->surface->setElevation(tile->renderModel.elevation.image, tile->renderModel.elevation.matrix);
            tile->bound = tile->surface->recomputeBound();
            factory->host->activity().onTileBoundsChanged.fire(tile->key);
            context->requestFrame();
            return true;
        };
        auto operation = PromiseOperation<bool>::create(merge);
        info.vectorMerger = operation->future();
        context->scheduleMeteredUpdate(operation, getPriority);
        context->requestFrame();
        changed = true;
    }
    // This is a sharing table for current terrain demand, not a second resident cache.
    // Releasing its last future cooperatively cancels obsolete work on the terrain loader.
    for (auto i = _vectorLoads.begin(); i != _vectorLoads.end(); )
        if (i->second.lastUsed != frame) i = _vectorLoads.erase(i);
        else ++i;
    return changed;
#endif
}
