/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainEngine.h"
#include "TerrainTileNode.h"
#include "GeometryPool.h"
#include "SurfaceNode.h"

#include <rocky/Map.h>
#include <rocky/Utils.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace ROCKY_NAMESPACE;


TerrainEngine::TerrainEngine(
    std::shared_ptr<Map> new_map,
    const SRS& new_worldSRS,
    Runtime& new_runtime,
    const TerrainSettings& new_settings) :

    map(new_map),
    worldSRS(new_worldSRS),
    runtime(new_runtime),
    settings(new_settings),
    geometryPool(worldSRS),
    stateFactory(new_runtime)
{
    jobs::get_pool(loadSchedulerName)->set_concurrency(settings.concurrency);

    ROCKY_SOFT_ASSERT(runtime.readerWriterOptions, "TerrainEngine requires readerWriterOptions");
    if (runtime.readerWriterOptions)
    {
        _tileLoader = TerrainTileNodeQuadReader::create();
        runtime.readerWriterOptions->readerWriters.emplace_back(_tileLoader);
    }
}

void
TerrainEngine::updateRenderModel(TerrainTileNode* tile, const TerrainTileModel& dataModel)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, void());

    bool updated = false;

    auto& renderModel = tile->renderModel;

    if (dataModel.colorLayers.size() > 0)
    {
        auto& layer = dataModel.colorLayers[0];
        if (layer.image.valid())
        {
            renderModel.color.name = "color " + layer.key.str();
            renderModel.color.image = layer.image.image();
            renderModel.color.matrix = layer.matrix;
        }
        updated = true;
    }

    if (dataModel.elevation.heightfield.valid())
    {
        renderModel.elevation.name = "elevation " + dataModel.elevation.key.str();
        renderModel.elevation.image = dataModel.elevation.heightfield.heightfield();
        renderModel.elevation.matrix = dataModel.elevation.matrix;

        // prompt the tile can update its bounds
        tile->surface->setElevation(renderModel.elevation.image, renderModel.elevation.matrix);

        updated = true;
    }

    if (dataModel.normalMap.image.valid())
    {
        renderModel.elevation.name = "normal " + dataModel.normalMap.key.str();
        renderModel.normal.image = dataModel.normalMap.image.image();
        renderModel.normal.matrix = dataModel.normalMap.matrix;

        updated = true;
    }

    renderModel.modelMatrix = to_glm(tile->surface->matrix);

    if (updated)
    {
        stateFactory.updateTerrainTileDescriptors(tile->renderModel, tile->stategroup, runtime);
    }
}

vsg::ref_ptr<TerrainTileNode>
TerrainEngine::createTile(const TileKey& key, Cancelable& cancelable)
{
    auto tile = TerrainTileNode::create();

    TerrainTileModelFactory factory;
    GeometryPool::Settings geometry_settings;

    IOOptions io(map->instance().io(), cancelable);

    auto dataModel = factory.createTileModel(map.get(), key, {}, io);
    if (dataModel.empty())
        return {};

    tile->key = key;
    tile->options = runtime.readerWriterOptions;
    tile->renderModel.descriptors = stateFactory.defaultTileDescriptors;

    auto geometry = geometryPool.getPooledGeometry(tile->key, geometry_settings, &cancelable);

    tile->surface = SurfaceNode::create(tile->key, SRS::ECEF);
    tile->children[1].node = tile->surface;
    tile->children[1].minimumScreenHeightRatio = 0.0f;

    if (key.levelOfDetail() < settings.maxLevelOfDetail)
    {
        tile->filename = TerrainTileNodeQuadReader::makePath(tile->key);
        tile->children[0].minimumScreenHeightRatio = 0.25f;
    }

    tile->stategroup = vsg::StateGroup::create();
    tile->surface->addChild(tile->stategroup);
    tile->stategroup->addChild(geometry);

    // TODO: if the data model is empty, make a completely-inherited tile with no
    // pagedlod filename so it cannot load children.

    updateRenderModel(tile, dataModel);

    tile->surface->recomputeBound();
    tile->bound = tile->surface->worldBoundingSphere;

    std::unique_lock lock(_weakTileCache_mutex);
    _weakTileCache[key] = tile;

    return tile;
}

vsg::ref_ptr<TerrainTileNode>
TerrainEngine::inheritTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent, Cancelable& cancelable)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(key.valid(), {});
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, {});
    ROCKY_SOFT_ASSERT_AND_RETURN(parent->key.valid(), {});
    ROCKY_SOFT_ASSERT_AND_RETURN(parent->key == key.createParentKey(), {});

    auto tile = TerrainTileNode::create();

    tile->key = key;
    tile->options = runtime.readerWriterOptions;

    TerrainTileModelFactory factory;
    GeometryPool::Settings geometry_settings;

    IOOptions io(map->instance().io(), cancelable);

    auto geometry = geometryPool.getPooledGeometry(tile->key, geometry_settings, &cancelable);

    tile->surface = SurfaceNode::create(tile->key, SRS::ECEF);
    tile->children[1].node = tile->surface;
    tile->children[1].minimumScreenHeightRatio = 0.0f;

    if (key.levelOfDetail() < settings.maxLevelOfDetail)
    {
        tile->filename = TerrainTileNodeQuadReader::makePath(tile->key);
        tile->children[0].minimumScreenHeightRatio = 0.25f;
    }

    tile->stategroup = vsg::StateGroup::create();
    tile->surface->addChild(tile->stategroup);
    tile->stategroup->addChild(geometry);

    tile->surface->recomputeBound();
    tile->bound = tile->surface->worldBoundingSphere;

    tile->renderModel = stateFactory.inheritTerrainTileDescriptors(
        parent->renderModel, key.scaleBiasMatrix(), tile->stategroup, runtime);

    std::unique_lock lock(_weakTileCache_mutex);
    _weakTileCache[key] = tile;

    return tile;
}

bool
TerrainEngine::updateTile(vsg::ref_ptr<TerrainTileNode> tile, Cancelable& cancelable)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(tile, false);
    ROCKY_SOFT_ASSERT_AND_RETURN(tile->key.valid(), false);

    TerrainTileModelFactory factory;
    GeometryPool::Settings geometry_settings;

    IOOptions io(map->instance().io(), cancelable);

    auto dataModel = factory.createTileModel(map.get(), tile->key, {}, io);
    if (dataModel.empty())
    {
        // no data model - tile cannot have any children.
        tile->filename.clear();
        tile->children[0].minimumScreenHeightRatio = 0.0f;
        tile->children[1].minimumScreenHeightRatio = FLT_MAX; // always show
        return false;
    }

    updateRenderModel(tile, dataModel);

    tile->surface->recomputeBound();
    tile->bound = tile->surface->worldBoundingSphere;

    return true;
}

bool
TerrainEngine::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
    geometryPool.sweep(runtime);

    // Tell the loader what frame we're on so it can process cancelations.
    auto* loader = static_cast<TerrainTileNodeQuadReader*>(_tileLoader.get());
    if (loader)
    {
        loader->tick(fs);
    }

    return true;
}

vsg::ref_ptr<TerrainTileNode> TerrainEngine::getCachedTile(const TileKey& key) const
{
    std::shared_lock lock(_weakTileCache_mutex);
    auto iter = _weakTileCache.find(key);
    if (iter != _weakTileCache.end()) return iter->second; else return {};
}
