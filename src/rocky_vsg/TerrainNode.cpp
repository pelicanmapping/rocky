/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "TerrainContext.h"
#include <rocky/IOTypes.h>
#include <rocky/Map.h>
#include <rocky/TileKey.h>

#include <vsg/all.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define ARENA_LOAD_TILE "terrain.load_tile"

TerrainNode::TerrainNode(RuntimeContext& new_runtime, const Config& conf) :
    vsg::Inherit<vsg::Group, TerrainNode>(),
    TerrainSettings(conf),
    _runtime(new_runtime)
{
    construct(conf);
}

void
TerrainNode::construct(const Config& conf)
{
    //nop
}

Config
TerrainNode::getConfig() const
{
    Config conf("terrain");
    TerrainSettings::saveToConfig(conf);
    return conf;
}

void
TerrainNode::setMap(shared_ptr<Map> new_map)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(new_map, void());

    // create a new context for this map
    _context = std::make_shared<TerrainContext>(
        new_map,
        _runtime, // runtime API
        *this,    // settings
        this);    // host

    // remove everything and start over
    this->children.clear();

    _tilesRoot = vsg::Group::create();

    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(this->firstLOD, new_map->profile(), keys);

    for (unsigned i = 0; i < keys.size(); ++i)
    {
        auto tile = _context->tiles->createTile(
            keys[i],
            nullptr, // parent
            _context);

        tile->doNotExpire = true;

        // Add it to the scene graph
        _tilesRoot->addChild(tile);
    }

    // create the graphics pipeline to render this map
    auto stateGroup = _context->stateFactory->createTerrainStateGroup();
    stateGroup->addChild(_tilesRoot);
    this->addChild(stateGroup);
}

void
TerrainNode::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
    _context->tiles->update(fs, io, _context);
}

void
TerrainNode::ping(
    TerrainTileNode* tile0,
    TerrainTileNode* tile1, 
    TerrainTileNode* tile2, 
    TerrainTileNode* tile3,
    vsg::RecordTraversal& nv)
{
    _context->tiles->ping(tile0, tile1, tile2, tile3, nv);
}
