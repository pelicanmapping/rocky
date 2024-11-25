/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTilePager.h"
#include "TerrainEngine.h"
#include "TerrainSettings.h"
#include "../Runtime.h"
#include "../Utils.h"

#include <rocky/ElevationLayer.h>
#include <rocky/ImageLayer.h>
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/nodes/QuadGroup.h>
#include <vsg/ui/FrameStamp.h>

using namespace ROCKY_NAMESPACE;

#define LC "[TerrainTilePager] "

#define RP_DEBUG if(false) Log()->info

//----------------------------------------------------------------------------

TerrainTilePager::TerrainTilePager(
    const Profile& profile, const TerrainSettings& settings,
    Runtime& runtime, TerrainTileHost* in_host) :
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

    auto& info = _tiles[tile->key];
    if (!info.tile)
    {
        info.tile = tile;
        info.trackerToken = _tracker.use(tile, nullptr);
    }
    else
    {
        _tracker.use(tile, info.trackerToken);
    }

    // next, see if the tile needs anything.
    // 
    // "progressive" means do not load LOD N+1 until LOD N is complete.
    const bool progressive = true;

    if (progressive)
    {
        if (info.dataMerger.available() && tile->needsSubtiles)
        {
            _loadSubtiles.push_back(tile->key);
        }

        if (parent == nullptr)
        {
            // root tile, go for it
            if (info.dataLoader.empty())
                _loadData.emplace_back(tile->key);
        }
        else
        {
            // non-root tile; make sure parent has its data loaded first
            auto& parent_info = _tiles[parent->key];
            ROCKY_SOFT_ASSERT_AND_RETURN(parent_info.tile, void());
            if (parent_info.dataMerger.available() && info.dataLoader.empty())
                _loadData.push_back(tile->key);
        }
    }

    // This will only queue one merge per frame, to prevent overloading
    // the (synchronous) update cycle in VSG.
    if (info.dataLoader.available() && info.dataMerger.empty())
        _mergeData.push_back(tile->key);

    if (tile->needsUpdate)
        _updateData.push_back(tile->key);

    if (_settings.supportMultiThreadedRecord)
        _mutex.unlock();
}

bool
TerrainTilePager::update(const vsg::FrameStamp* fs, const IOOptions& io, std::shared_ptr<TerrainEngine> terrain)
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
            if (iter->second.tile->update(fs, io))
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
            requestLoadSubtiles(iter->second.tile, terrain); // parent, context
            iter->second.tile->needsSubtiles = false;
        }

        changes = true;
    }
    _loadSubtiles.clear();

    // launch any data loading requests
    for (auto& key : _loadData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestLoadData(iter->second, io, terrain);
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
            requestMergeData(iter->second, io, terrain);
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
                                terrain->runtime.dispose(tile->children[1]);

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

        _tracker.flush(~0, dispose);
    }

    // synchronize
    _lastUpdate = fs->frameCount;

    return changes;
}

vsg::ref_ptr<TerrainTileNode>
TerrainTilePager::createTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent, std::shared_ptr<TerrainEngine> terrain)
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
    auto tile = TerrainTileNode::create();
    tile->key = key;
    tile->renderModel.descriptors = terrain->stateFactory.defaultTileDescriptors;
    tile->doNotExpire = (parent == nullptr);
    tile->stategroup = vsg::StateGroup::create();
    tile->stategroup->addChild(geometry);
    tile->surface = SurfaceNode::create(key, terrain->worldSRS);
    tile->surface->addChild(tile->stategroup);
    tile->addChild(tile->surface);
    tile->host = terrain->tiles._host;

    // inherit model data from the parent
    if (parent)
        tile->inheritFrom(parent);

    // update the bounding sphere for culling
    tile->bound = tile->surface->recomputeBound();

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
        iter != _tiles.end() ? iter->second.tile :
        vsg::ref_ptr<TerrainTileNode>(nullptr);
}
void
TerrainTilePager::requestLoadSubtiles(
    vsg::ref_ptr<TerrainTileNode> parent,
    std::shared_ptr<TerrainEngine> engine) const
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

                auto tile = engine->tiles.createTile(childkey, parent, engine);

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
    TableEntry& info,
    const IOOptions& in_io,
    std::shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile, void());

    // make sure we're not already working on it
    if (info.dataLoader.working() || info.dataLoader.available())
    {
        return;
    }

    auto key = info.tile->key;

    //RP_DEBUG("requestLoadData -> {}", key.str());

    CreateTileManifest manifest;
    const IOOptions io(in_io);

    auto load = [key, manifest, engine, io](Cancelable& p) -> TerrainTileModel
    {
        if (p.canceled())
        {
            //RP_DEBUG("Data load {} Canceled!", key.str());
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
    vsg::observer_ptr<TerrainTileNode> tile_weak(info.tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
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
TerrainTilePager::requestMergeData(
    TableEntry& info,
    const IOOptions& in_io,
    std::shared_ptr<TerrainEngine> engine) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(info.tile, void());

    // make sure we're not already working on it
    if (info.dataMerger.working() || info.dataMerger.available())
    {
        return;
    }

    auto key = info.tile->key;
    auto model = info.dataLoader.value();

    const IOOptions io(in_io);

    //RP_DEBUG("requestMergeData -> {}", key.str());

    auto merge = [key, model, engine](Cancelable& p) -> bool
    {
        if (p.canceled())
        {
            //Log()->info("  merge canceled -> {}", key.str());
            return false;
        }

        auto tile = engine->tiles.getTile(key);
        if (!tile)
        {
            //Log()->info("  merge tile lost -> {}", key.str());
            return false;
        }

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

        if (model.elevation.heightfield.valid())
        {
            renderModel.elevation.name = "elevation " + model.elevation.key.str();
            renderModel.elevation.image = model.elevation.heightfield.heightfield();
            renderModel.elevation.matrix = model.elevation.matrix;

            // update the tile proxy geometry and bounds
            tile->surface->setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
            tile->bound = tile->surface->recomputeBound();

            updated = true;
        }

        if (model.normalMap.image.valid())
        {
            renderModel.elevation.name = "normal " + model.normalMap.key.str();
            renderModel.normal.image = model.normalMap.image.image();
            renderModel.normal.matrix = model.normalMap.matrix;

            updated = true;
        }

        renderModel.modelMatrix = to_glm(tile->surface->matrix);

        if (updated)
        {
            engine->stateFactory.updateTerrainTileDescriptors(
                renderModel,
                tile->stategroup,
                engine->runtime);

            RP_DEBUG("  merge ok -> {}", key.str());
        }
        else
        {
            RP_DEBUG("  merge empty -> {}", key.str());
        }

        // GW: trying this...
        engine->runtime.compile(tile);

        engine->runtime.requestFrame();

        return true;
    };

    auto merge_op = util::PromiseOperation<bool>::create(merge);

    info.dataMerger = merge_op->future();

    vsg::observer_ptr<TerrainTileNode> tile_weak(info.tile);
    auto priority_func = [tile_weak]() -> float
    {
        vsg::ref_ptr<TerrainTileNode> tile = tile_weak.ref_ptr();
        return tile ? -(sqrt(tile->lastTraversalRange) * tile->key.levelOfDetail()) : 0.0f;
    };

    engine->runtime.onNextUpdate(merge_op, priority_func);
}
