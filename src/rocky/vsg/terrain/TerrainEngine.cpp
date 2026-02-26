/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TerrainEngine.h"
#include "TerrainTilePager.h"
#include "TerrainTileHost.h"
#include "TerrainSettings.h"
#include "TerrainState.h"
#include "SurfaceNode.h"
#include "GeometryPool.h"
#include <rocky/TerrainTileModel.h>
#include <rocky/Map.h>

using namespace ROCKY_NAMESPACE;


TerrainEngine::TerrainEngine(
    std::shared_ptr<const Map> new_map,
    const Profile& new_profile,
    const SRS& new_renderingSRS,
    TerrainState& new_stateFactory,
    //VSGContext new_context,
    const TerrainSettings& new_settings,
    TerrainTileHost* new_host) :

    map(new_map),
    profile(new_profile),
    renderingSRS(new_renderingSRS),
    stateFactory(new_stateFactory),
    //context(new_context),
    settings(new_settings),
    geometryPool(new_renderingSRS),
    host(new_host)
{
    ROCKY_SOFT_ASSERT(map, "Map is required");
    ROCKY_SOFT_ASSERT(profile.valid(), "Valid profile required");

    jobs::get_pool(loadSchedulerName)->set_concurrency(settings.concurrency);

    // geometry pooling not supported for QSC yet.
    if (new_profile.srs().isQSC())
    {
        geometryPool.enabled = false;
    }
}

TerrainEngine::~TerrainEngine()
{
#ifdef ROCKY_DEBUG_MEMCHECK
    Log()->debug("~TerrainEngine");
#endif
}

vsg::ref_ptr<TerrainTileNode>
TerrainEngine::createTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent, VSGContext context)
{
    GeometryPool::Settings geomSettings
    {
        settings.tileSize,
        settings.skirtRatio,
        false // morphing
    };

    // Get a shared geometry from the pool that corresponds to this tile key:
    auto geometry = geometryPool.getPooledGeometry(key, geomSettings, nullptr);

    // Make the new terrain tile
    auto tile = TerrainTileNode::create();
    tile->key = key;
    tile->renderModel.descriptors = stateFactory.defaultTileDescriptors;
    tile->doNotExpire = (parent == nullptr);
    tile->stategroup = vsg::StateGroup::create();
    tile->stategroup->addChild(geometry);
    tile->surface = SurfaceNode::create(key, renderingSRS);
    tile->surface->addChild(tile->stategroup);
    tile->addChild(tile->surface);
    tile->host = host;

    // inherit model data from the parent
    if (parent)
        tile->inheritFrom(parent);

    // update the bounding sphere for culling
    tile->bound = tile->surface->recomputeBound();

    // Generate its state objects:
    tile->renderModel = stateFactory.updateRenderModel(tile->renderModel, {}, context);

    // install the bind command.
    tile->stategroup->add(tile->renderModel.descriptors.bind);

    return tile;
}

bool TerrainEngine::update(VSGContext context)
{
    bool changes = false;

    geometryPool.sweep(context);

    auto pool = jobs::get_pool(loadSchedulerName);
    if (pool->concurrency() != settings.concurrency)
    {
        pool->set_concurrency(settings.concurrency);
        changes = true;
    }

    return changes;
}
