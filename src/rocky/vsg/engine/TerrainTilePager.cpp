/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTilePager.h"
#include "TerrainEngine.h"
#include "Runtime.h"
#include "Utils.h"

#include <rocky/vsg/TerrainSettings.h>

#include <rocky/ElevationLayer.h>
#include <rocky/ImageLayer.h>
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/nodes/QuadGroup.h>
#include <vsg/ui/FrameStamp.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TerrainTilePager] "

//#define LOAD_ELEVATION_SEPARATELY

//#define RP_DEBUG Log::info()
#define RP_DEBUG if(false) Log::info()

//----------------------------------------------------------------------------

TerrainTilePager::TerrainTilePager(
    const Profile& profile,
    const TerrainSettings& settings,
    Runtime& runtime,
    TerrainTileHost* in_host) :

    _host(in_host),
    _settings(settings),
    _runtime(runtime)
{
    _firstLOD = settings.minLevelOfDetail;
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
    _loadSubtiles.clear();
    _loadElevation.clear();
    _mergeElevation.clear();
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

#if 0
        auto tileHasData = tile->dataMerger.available();

        if (tileHasData && tile->_needsChildren)
            _loadChildren.push_back(tile->key);

        if (parentHasData && tile->dataLoader.empty())
            _loadData.push_back(tile->key);
#else

        auto tileHasData = tile->dataMerger.available();

#ifdef LOAD_ELEVATION_SEPARATELY
        auto tileHasElevation = tile->elevationMerger.available();
#else
        auto tileHasElevation = tileHasData;
#endif
        
        if (tileHasData && tileHasElevation && tile->_needsSubtiles)
            _loadSubtiles.push_back(tile->key);

#ifdef LOAD_ELEVATION_SEPARATELY
        bool parentHasElevation = (parent == nullptr || parent->elevationMerger.available());
        if (parentHasElevation && tile->elevationLoader.empty())
            _loadElevation.push_back(tile->key);
#endif

        bool parentHasData = (parent == nullptr || parent->dataMerger.available());
        if (parentHasData && tile->dataLoader.empty())
            _loadData.push_back(tile->key);
#endif

    }
    else
    {
        //// free for all
        //if (tile->_needsChildren)
        //    _needsChildren.push_back(tile->key);

        //if (tile->dataLoader.empty())
        //    _needsLoad.push_back(tile->key);
    }

#ifdef LOAD_ELEVATION_SEPARATELY
    if (tile->elevationLoader.available() && tile->elevationMerger.empty())
        _mergeElevation.push_back(tile->key);
#endif

    // This will only queue one merge per frame, to prevent overloading
    // the (synchronous) update cycle in VSG.
    if (tile->dataLoader.available() && tile->dataMerger.empty())
        _mergeData.push_back(tile->key);

    if (tile->_needsUpdate)
        _updateData.push_back(tile->key);

    if (_settings.supportMultiThreadedRecord)
        _mutex.unlock();
}

bool
TerrainTilePager::update(const vsg::FrameStamp* fs, const IOOptions& io, shared_ptr<TerrainEngine> terrain)
{
    std::scoped_lock lock(_mutex);

    bool changes = false;

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
            if (iter->second._tile->update(fs, io))
                changes = true;
        }
    }
    _updateData.clear();

    // launch any "new subtiles" requests
    for (auto& key : _loadSubtiles)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadSubtiles(
                iter->second._tile, // parent
                terrain);  // context

            iter->second._tile->_needsSubtiles = false;
        }

        changes = true;
    }
    _loadSubtiles.clear();

#ifdef LOAD_ELEVATION_SEPARATELY
    // launch any data loading requests
    for (auto& key : _loadElevation)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadElevation(iter->second._tile, io, terrain);
        }

        changes = true;
    }
    _loadElevation.clear();

    // schedule any data merging requests
    for (auto& key : _mergeElevation)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestMergeElevation(iter->second._tile, io, terrain);
        }

        changes = true;
    }
    _mergeElevation.clear();
#endif

    // launch any data loading requests
    for (auto& key : _loadData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadData(iter->second._tile, io, terrain);
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
            requestMergeData(iter->second._tile, io, terrain);
        }

        changes = true;
    }
    _mergeData.clear();

    // Flush unused tiles (i.e., tiles that failed to ping) out of the system.
    // Tiles ping their children all at once; this should in theory prevent
    // a child from expiring without its siblings.
    bool disposeOrphanedTiles = (fs->frameCount - _lastTrackerFlushFrame) >= 2;

    const auto dispose = [&](TerrainTileNode* tile)
        {
            if (disposeOrphanedTiles && !tile->doNotExpire)
            {
                auto key = tile->key;
                auto parent_iter = _tiles.find(key.createParentKey());
                if (parent_iter != _tiles.end())
                {
                    auto parent = parent_iter->second._tile;
                    if (parent.valid())
                    {
                        parent->unloadSubtiles(terrain->runtime);
                    }
                }
                _tiles.erase(key);
                return true;
            }
            return false;
        };

    _tracker.flush(~0, dispose);

    // synchronize
    _lastTrackerFlushFrame = fs->frameCount;

    return changes;
}

vsg::ref_ptr<TerrainTileNode>
TerrainTilePager::createTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent, shared_ptr<TerrainEngine> terrain)
{
    GeometryPool::Settings geomSettings
    {
        terrain->settings.tileSize,
        terrain->settings.skirtRatio,
        false // morphing
    };

    // Get a shared geometry from the pool that corresponds to this tile key:
    auto geometry = terrain->geometryPool.getPooledGeometry(key, geomSettings, nullptr);

    // Make the new terrain tile
    auto tile = TerrainTileNode::create(
        key,
        parent,
        geometry,
        terrain->worldSRS,
        terrain->stateFactory.defaultTileDescriptors,
        terrain->tiles._host,
        terrain->runtime);

    // inherit model data from the parent
    if (parent)
        tile->inheritFrom(parent);

    // update the bounding sphere for culling
    tile->recomputeBound();

    // Generate its state group:
    terrain->stateFactory.updateTerrainTileDescriptors(
        tile->renderModel,
        tile->stategroup,
        terrain->runtime);

    return tile;
}

vsg::ref_ptr<TerrainTileNode>
TerrainTilePager::getTile(const TileKey& key) const
{
    std::scoped_lock lock(_mutex);
    auto iter = _tiles.find(key);
    return
        iter != _tiles.end() ? iter->second._tile :
        vsg::ref_ptr<TerrainTileNode>(nullptr);
}
void
TerrainTilePager::requestLoadSubtiles(
    vsg::ref_ptr<TerrainTileNode> parent,
    shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    // make sure we're not already working on it
    if (!parent->subtilesLoader.empty())
        return;

    //RP_DEBUG << "requestLoadSubtiles -> " << parent->key.str() << std::endl;

    vsg::observer_ptr<TerrainTileNode> weak_parent(parent);

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

                auto tile = engine->tiles.createTile(
                    childkey,
                    parent,
                    engine);

                ROCKY_SOFT_ASSERT_AND_RETURN(tile != nullptr, result);

                quad->children[quadrant] = tile;
            }

            // assign the result once all 4 children are added
            result = quad;
        }

        engine->runtime.requestFrame();

        return result;
    };

    // a callback that will return the loading priority of a tile
    auto priority_func = [weak_parent]() -> float
    {
        auto tile = weak_parent.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    parent->subtilesLoader = engine->runtime.compileAndAddChild(
        parent,
        create_children,
        jobs::context {
            "create child " + parent->key.str(),
            jobs::get_pool(engine->loadSchedulerName),
            priority_func,
            nullptr
        });
}

void
TerrainTilePager::requestLoadData(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataLoader.working() || tile->dataLoader.available())
    {
        return;
    }

    auto key = tile->key;

    //Log()->debug("requestLoadData -> " + key.str());

    CreateTileManifest manifest;

#ifdef LOAD_ELEVATION_SEPARATELY
    for (auto& layer : engine->map->layers().ofType<ImageLayer>())
        manifest.insert(layer);
#endif

    const IOOptions io(in_io);

    auto load = [key, manifest, engine, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            //RP_DEBUG << "Data load " << key.str() << " CANCELED!" << std::endl;
            return { };
        }

        TerrainTileModelFactory factory;

        factory.compositeColorLayers = true;

        auto model = factory.createTileModel(
            engine->map.get(),
            key,
            manifest,
            IOOptions(io, p));

        engine->runtime.requestFrame();

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

    tile->dataLoader = jobs::dispatch(
        load, 
        jobs::context {
            "load data " + key.str(),
            jobs::get_pool(engine->loadSchedulerName),
            priority_func,
            nullptr
        } );
}

void
TerrainTilePager::requestMergeData(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->dataMerger.working() || tile->dataMerger.available())
    {
        return;
    }

    auto key = tile->key;
    const IOOptions io(in_io);

    //RP_DEBUG << "requestMergeData -> " << key.str() << std::endl;

    auto merge = [key, engine](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            //RP_DEBUG << "merge CANCELED -> " << key.str() << std::endl;
            return false;
        }

        auto tile = engine->tiles.getTile(key);
        if (!tile)
        {
            //RP_DEBUG << "merge TILE LOST -> " << key.str() << std::endl;
            return false;
        }

        auto model = tile->dataLoader.value();

        auto& renderModel = tile->renderModel;

        bool updated = false;

        if (model.colorLayers.size() > 0)
        {
            auto& layer = model.colorLayers[0];
            if (layer.image.valid())
            {
                renderModel.color.name = "color " + layer.key.str();
                renderModel.color.image = layer.image.image();
                renderModel.color.matrix = layer.matrix;
            }
            updated = true;
        }

#ifndef LOAD_ELEVATION_SEPARATELY
        if (model.elevation.heightfield.valid())
        {
            renderModel.elevation.name = "elevation " + model.elevation.key.str();
            renderModel.elevation.image = model.elevation.heightfield.heightfield();
            renderModel.elevation.matrix = model.elevation.matrix;

            // prompt the tile can update its bounds
            tile->setElevation(
                renderModel.elevation.image,
                renderModel.elevation.matrix);

            updated = true;
        }

        if (model.normalMap.image.valid())
        {
            renderModel.elevation.name = "normal " + model.normalMap.key.str();
            renderModel.normal.image = model.normalMap.image.image();
            renderModel.normal.matrix = model.normalMap.matrix;

            updated = true;
        }
#endif

        renderModel.modelMatrix = to_glm(tile->surface->matrix);

        if (updated)
        {
            engine->stateFactory.updateTerrainTileDescriptors(
                renderModel,
                tile->stategroup,
                engine->runtime);

            //RP_DEBUG << "mergeData -> " << key.str() << std::endl;
        }
        else
        {
            //RP_DEBUG << "merge EMPTY TILE MODEL -> " << key.str() << std::endl;
        }

        engine->runtime.requestFrame();

        return true;
    };

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    tile->dataMerger = merge_op->future();

    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    engine->runtime.onNextUpdate(merge_op, priority_func);
}

void
TerrainTilePager::requestLoadElevation(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainEngine> engine) const
{
#ifdef LOAD_ELEVATION_SEPARATELY
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->elevationLoader.working() || tile->elevationLoader.available())
    {
        return;
    }

    auto key = tile->key;

    CreateTileManifest manifest;
    for (auto& elev : engine->map->layers().ofType<ElevationLayer>())
        manifest.insert(elev);

    const IOOptions io(in_io);

    auto load = [key, manifest, engine, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            return { };
        }

        TerrainTileModelFactory factory;

        auto model = factory.createTileModel(
            engine->map.get(),
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
        return tile ? -(sqrt(tile->lastTraversalRange) * 0.9 * tile->key.levelOfDetail()) : 0.0f;
    };

    tile->elevationLoader = jobs::dispatch(
        load, {
             "load elevation " + key.str(),
             priority_func,
             util::job_scheduler::get(engine->loadSchedulerName),
             nullptr
        }
    );
#endif
}

void
TerrainTilePager::requestMergeElevation(
    vsg::ref_ptr<TerrainTileNode> tile,
    const IOOptions& in_io,
    shared_ptr<TerrainEngine> engine) const
{
#ifdef LOAD_ELEVATION_SEPARATELY
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    // make sure we're not already working on it
    if (tile->elevationMerger.working() || tile->elevationMerger.available())
    {
        return;
    }

    auto key = tile->key;
    const IOOptions io(in_io);

    auto merge = [key, engine](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            return false;
        }

        auto tile = engine->tiles.getTile(key);
        if (tile)
        {
            auto model = tile->elevationLoader.value();

            auto& renderModel = tile->renderModel;

            bool updated = false;

            if (model.elevation.heightfield.valid())
            {
                renderModel.elevation.image = model.elevation.heightfield.heightfield();
                renderModel.elevation.matrix = model.elevation.matrix;

                // prompt the tile can update its bounds
                tile->setElevation(
                    renderModel.elevation.image,
                    renderModel.elevation.matrix);

                updated = true;
            }

            if (model.normalMap.image.valid())
            {
                renderModel.normal.image = model.normalMap.image.image();
                renderModel.normal.matrix = model.normalMap.matrix;
                updated = true;
            }

            if (updated)
            {
                engine->stateFactory.updateTerrainTileDescriptors(
                    renderModel,
                    tile->stategroup,
                    engine->runtime);

                Log::info() << "Elevation merged for " << key.str() << std::endl;
            }
        }

        return true;
    };

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    tile->elevationMerger = merge_op->future();

    vsg::observer_ptr<TerrainTileNode> tile_weak(tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * 0.9 * tile->key.levelOfDetail()) : 0.0f;
    };
    engine->runtime.onNextUpdate(merge_op, priority_func);
#endif
}
