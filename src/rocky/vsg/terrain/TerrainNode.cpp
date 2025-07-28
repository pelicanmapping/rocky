/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "TerrainEngine.h"

#include <rocky/json.h>
#include <rocky/IOTypes.h>
#include <rocky/Map.h>
#include <rocky/TileKey.h>

#include <vsg/all.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

Result<>
TerrainNode::from_json(const std::string& JSON, const IOOptions& io)
{
    return TerrainSettings::from_json(JSON);
}

std::string
TerrainNode::to_json() const
{
    return TerrainSettings::to_json();
}

const Result<>&
TerrainNode::setMap(std::shared_ptr<Map> new_map, const Profile& new_profile, VSGContext& context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(new_map, status);

    // remove old hooks:
    if (map)
    {
        _callbacks.clear();
    }

    map = new_map;
    profile = new_profile;

    if (map)
    {
        _callbacks.emplace(
            map->onLayersChanged([this, context](auto...) { reset(context); }));
    }

    reset(context);

    return status;
}

void
TerrainNode::reset(VSGContext context)
{
    for(auto& child : this->children)
    {
        context->dispose(child);
    }

    children.clear();

    // create a new engine to render this map
    engine = std::make_shared<TerrainEngine>(
        map,
        profile,
        context,   // runtime API
        *this,     // settings
        this);     // host

    status = engine->stateFactory.status;
}

Result<>
TerrainNode::createRootTiles(VSGContext& context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(engine != nullptr, Failure_AssertionFailure);
    ROCKY_SOFT_ASSERT_AND_RETURN(engine->stateFactory.status.ok(), engine->stateFactory.status);
    ROCKY_HARD_ASSERT(children.empty(), "TerrainNode::createRootTiles() called with children already present");

    tilesRoot = vsg::Group::create();

    // create the graphics pipeline to render this map
    stategroup = engine->stateFactory.createTerrainStateGroup(engine->context);
    stategroup->addChild(tilesRoot);
    this->addChild(stategroup);

    // once the pipeline exists, we can start creating tiles.
    auto keys = engine->profile.allKeysAtLOD(this->minLevelOfDetail);

    for(auto& key : keys)
    {
        // create a tile with no parent:
        auto tile = engine->createTile(key, {});

        // ensure it can't page out:
        tile->doNotExpire = true;

        // Add it to the scene graph
        tilesRoot->addChild(tile);
    }

    engine->context->compile(stategroup);

    return {};
}

bool
TerrainNode::update(VSGContext context)
{
    bool changes = false;

    if (status.ok())
    {
        if (children.empty())
        {
            status = createRootTiles(context);

            if (status.failed())
            {
                Log()->warn("TerrainNode initialize failed: " + status.error().message);
            }
            changes = true;
        }
        else
        {
            ROCKY_HARD_ASSERT(engine);

            if (engine->tiles.update(context->viewer->getFrameStamp(), context->io, engine))
                changes = true;
            
            engine->geometryPool.sweep(engine->context);
        }
    }

    return changes;
}

void
TerrainNode::ping(TerrainTileNode* tile, const TerrainTileNode* parent, vsg::RecordTraversal& nv)
{
    engine->tiles.ping(tile, parent, nv);
}
