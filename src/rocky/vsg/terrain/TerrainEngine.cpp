/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainEngine.h"
#include "TerrainTilePager.h"
#include "TerrainTileHost.h"
#include "TerrainSettings.h"
#include "GeometryPool.h"
#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/state/ShaderStage.h>

using namespace ROCKY_NAMESPACE;


TerrainEngine::TerrainEngine(
    std::shared_ptr<Map> new_map,
    const Profile& new_profile,
    VSGContext& new_context,
    const TerrainSettings& new_settings,
    TerrainTileHost* host) :

    map(new_map),
    profile(new_profile),
    context(new_context),
    settings(new_settings),
    geometryPool(new_profile),
    tiles(new_profile, new_settings, new_context, host),
    stateFactory(new_context)
{
    ROCKY_SOFT_ASSERT(map, "Map is required");
    ROCKY_SOFT_ASSERT(profile.valid(), "Valid profile required");

    worldSRS = profile.srs().isGeodetic() ? profile.srs().geocentricSRS() : profile.srs();

    jobs::get_pool(loadSchedulerName)->set_concurrency(settings.concurrency);
}


vsg::ref_ptr<TerrainTileNode>
TerrainEngine::createTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent)
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
    tile->surface = SurfaceNode::create(key, worldSRS);
    tile->surface->addChild(tile->stategroup);
    tile->addChild(tile->surface);
    tile->host = tiles._host;

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