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

Status
TerrainNode::setMap(shared_ptr<Map> new_map, const SRS& new_worldSRS)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(new_map,
        Status(Status::ConfigurationError, "Null map"));

    SRS worldSRS = new_worldSRS;
    if (!worldSRS.valid())
    {
        worldSRS = new_map->srs().isGeographic() ?
            SRS::ECEF :
            new_map->srs();
    }

    // create a new context for this map
    _context = std::make_shared<TerrainContext>(
        new_map,
        worldSRS,
        _runtime, // runtime API
        *this,    // settings
        this);    // host

    // erase everything so the map will reinitialize
    this->children.clear();

    return StatusOK;
}

Status
TerrainNode::createRootTiles(const IOOptions& io)
{
    // remove everything and start over
    this->children.clear();

    // check that everything initialized ok
    if (_context->stateFactory->status.failed())
    {
        return _context->stateFactory->status;
    }

    _tilesRoot = vsg::Group::create();

    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(this->firstLOD, _context->map->profile(), keys);

    for (unsigned i = 0; i < keys.size(); ++i)
    {
        auto tile = _context->tiles->createTile(
            keys[i],
            { }, // parent
            _context);

        tile->doNotExpire = true;

        // Add it to the scene graph
        _tilesRoot->addChild(tile);
    }

    // create the graphics pipeline to render this map
    auto stateGroup = _context->stateFactory->createTerrainStateGroup();
    stateGroup->addChild(_tilesRoot);
    this->addChild(stateGroup);

    _context->runtime.compiler()->compile(stateGroup);

    return StatusOK;
}

void
TerrainNode::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
    if (children.empty())
    {
        auto s = createRootTiles(io);
        if (s.failed())
        {
            Log::warn() << "TerrainNode initialize failed: " << s.message << std::endl;
        }
    }
    else
    {
        _context->tiles->update(fs, io, _context);
    }
}

void
TerrainNode::ping(
    TerrainTileNode* parent,
    bool parentHasData,
    vsg::RecordTraversal& nv)
{
    _context->tiles->ping(parent, parentHasData, nv);
}
