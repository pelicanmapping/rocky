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
    _host(in_host),
    _mutex("TileNodeRegistry(OE)")
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
    util::ScopedLock lock(_mutex);
    
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
    util::ScopedLock lock(_mutex);

    _tiles.clear();
    _tracker.reset();
    _needsUpdate.clear();
    _needsData.clear();
    _needsChildren.clear();
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
                entry._trackerToken = _tracker.use(tile, nullptr);
            }
            else
            {
                _tracker.use(tile, i->second._trackerToken);
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
    const IOOptions& io,
    shared_ptr<TerrainContext> terrain)
{
    util::ScopedLock lock(_mutex);

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
    for (auto& key : _needsData)
    {
        auto iter = _tiles.find(key);
        if (iter != _tiles.end())
        {
            requestTileData(iter->second._tile.get(), io, terrain);
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
            ROCKY_DEBUG << "Removing tile and sibs " << tile->key.str() << std::endl;
            auto parent = tile->getParentTile();
            if (parent)
                parent->unloadChildren();
            _tiles.erase(tile->key);
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
        terrain->tiles->_host);

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
    util::ScopedLock lock(_mutex);
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

void
TileNodeRegistry::requestTileData(
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

    const auto load_async = [key, manifest, terrain, io](Cancelable& p) -> Result<TerrainTileModel>
    {
        if (p.canceled())
        {
            return Result<TerrainTileModel>();
        }

        TerrainTileModelFactory factory;

        auto model = factory.createTileModel(
            terrain->map.get(),
            key,
            manifest,
            IOOptions(io, p));

        return Result(model);
    };

    const auto merge_sync = [key, manifest, terrain](const TerrainTileModel& model, Cancelable& p)
    {
        if (p.canceled())
        {
            return;
        }

        auto tile = terrain->tiles->getTile(key);
        if (tile)
        {
            //ROCKY_INFO << "Merging data for tile " << key.str() << std::endl;
            //tile->merge(model, manifest);

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
    };

    tile->dataLoader = terrain->runtime.runAsyncUpdateSync<TerrainTileModel>(
        load_async,
        merge_sync);
}
