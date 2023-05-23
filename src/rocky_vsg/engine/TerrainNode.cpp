/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "TerrainEngine.h"
#include <rocky/IOTypes.h>
#include <rocky/Map.h>
#include <rocky/TileKey.h>

#include <vsg/all.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define ARENA_LOAD_TILE "terrain.load_tile"

TerrainNode::TerrainNode(Runtime& new_runtime, const JSON& conf) :
    vsg::Inherit<vsg::Group, TerrainNode>(),
    TerrainSettings(conf),
    _runtime(new_runtime)
{
    construct(conf);
}

void
TerrainNode::construct(const JSON& conf)
{
    //nop
}

JSON
TerrainNode::to_json() const
{
    return TerrainSettings::to_json();
}

const Status&
TerrainNode::setMap(shared_ptr<Map> new_map, const SRS& new_worldSRS)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(new_map, _status);

    _map = new_map;

    _worldSRS = new_worldSRS;
    if (!_worldSRS.valid())
    {
        _worldSRS = new_map->srs().isGeodetic() ?
            SRS::ECEF :
            new_map->srs();
    }

    _engine = nullptr;

    // erase everything so the map will reinitialize
    this->children.clear();
    _status = StatusOK;
    return status();
}

Status
TerrainNode::createRootTiles(const IOOptions& io)
{
    // remove everything and start over
    this->children.clear();

    // create a new context for this map
    _engine = std::make_shared<TerrainEngine>(
        _map,
        _worldSRS,
        _runtime,  // runtime API
        *this,     // settings
        this);     // host

    // check that everything initialized ok
    if (_engine->stateFactory.status.failed())
    {
        return _engine->stateFactory.status;
    }

    _tilesRoot = vsg::Group::create();

    // create the graphics pipeline to render this map
    auto stateGroup = _engine->stateFactory.createTerrainStateGroup();
    stateGroup->addChild(_tilesRoot);
    this->addChild(stateGroup);

    // once the pipeline exists, we can start creating tiles.
    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(this->minLevelOfDetail, _engine->map->profile(), keys);

    for (unsigned i = 0; i < keys.size(); ++i)
    {
        auto tile = _engine->tiles.createTile(
            keys[i],
            { }, // parent
            _engine);

        tile->doNotExpire = true;

        // Add it to the scene graph
        _tilesRoot->addChild(tile);
    }

    _engine->runtime.compile(stateGroup);

    //auto cr = _engine->runtime.viewer()->compileManager->compile(stateGroup);
    //if (cr && cr.requiresViewerUpdate())
    //    vsg::updateViewer(*_engine->runtime.viewer(), cr);

    return StatusOK;
}

void
TerrainNode::update(const vsg::FrameStamp* fs, const IOOptions& io)
{
    if (_status.ok())
    {
        if (children.empty())
        {
            _status = createRootTiles(io);
            if (_status.failed())
            {
                Log::warn() << "TerrainNode initialize failed: " << _status.message << std::endl;
            }
        }
        else
        {
            _engine->tiles.update(fs, io, _engine);
        }
    }
}

void
TerrainNode::ping(
    TerrainTileNode* tile,
    const TerrainTileNode* parent,
    vsg::RecordTraversal& nv)
{
    _engine->tiles.ping(tile, parent, nv);
}
