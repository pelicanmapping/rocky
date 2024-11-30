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

Status
TerrainNode::from_json(const std::string& JSON, const IOOptions& io)
{
    return TerrainSettings::from_json(JSON);
}

std::string
TerrainNode::to_json() const
{
    return TerrainSettings::to_json();
}

const Status&
TerrainNode::setMap(std::shared_ptr<Map> new_map, const SRS& new_worldSRS, VSGContext& context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(new_map, status);

    // remove old hooks:
    if (map)
    {
        map->onLayerAdded.remove(std::uintptr_t(this));
        map->onLayerRemoved.remove(std::uintptr_t(this));
    }

    map = new_map;

    worldSRS = new_worldSRS;
    if (!worldSRS.valid())
    {
        worldSRS = new_map->srs().isGeodetic() ?
            SRS::ECEF :
            new_map->srs();
    }

    if (map)
    {
        map->onLayerAdded([this, context](auto...) { reset(context); });
        map->onLayerRemoved([this, context](auto...) { reset(context); });
    }

    engine = nullptr;

    // erase everything so the map will reinitialize
    this->children.clear();
    status = StatusOK;
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

    engine = nullptr;

    status = Status_OK;
}

Status
TerrainNode::createRootTiles(VSGContext& context)
{
    ROCKY_HARD_ASSERT(children.empty(), "TerrainNode::createRootTiles() called with children already present");

    // create a new engine to render this map
    engine = std::make_shared<TerrainEngine>(
        map,
        worldSRS,
        context,   // runtime API
        *this,     // settings
        this);     // host

    // check that everything initialized ok
    if (engine->stateFactory.status.failed())
    {
        return engine->stateFactory.status;
    }

    tilesRoot = vsg::Group::create();

    // create the graphics pipeline to render this map
    stategroup = engine->stateFactory.createTerrainStateGroup(engine->context);
    stategroup->addChild(tilesRoot);
    this->addChild(stategroup);

    // once the pipeline exists, we can start creating tiles.
    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(this->minLevelOfDetail, engine->map->profile(), keys);

    for (unsigned i = 0; i < keys.size(); ++i)
    {
        // create a tile with no parent:
        auto tile = engine->tiles.createTile(keys[i], {}, engine);

        // ensure it can't page out:
        tile->doNotExpire = true;

        // Add it to the scene graph
        tilesRoot->addChild(tile);
    }

    engine->context->compile(stategroup);

    return StatusOK;
}

bool
TerrainNode::update(const vsg::FrameStamp* fs, VSGContext& context)
{
    bool changes = false;

    if (status.ok())
    {
        if (children.empty())
        {
            status = createRootTiles(context);
            if (status.failed())
            {
                Log()->warn("TerrainNode initialize failed: " + status.message);
            }
            changes = true;
        }
        else
        {
            ROCKY_HARD_ASSERT(engine);

            if (engine->tiles.update(fs, context->io, engine))
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
