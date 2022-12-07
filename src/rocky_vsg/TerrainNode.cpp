/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include <rocky/TileKey.h>
#include <rocky/Threading.h>
#include <rocky/ImageLayer.h>

#include <vsg/all.h>

using namespace rocky;
using namespace rocky::util;

#define ARENA_LOAD_TILE "terrain.load_tile"

TerrainNode::TerrainNode(RuntimeContext& rt, const Config& conf) :
    vsg::Inherit<vsg::Group, TerrainNode>(),
    TerrainContext(rt, conf)
{
    construct(conf);
}

void
TerrainNode::construct(const Config& conf)
{
    firstLOD.setDefault(0u);
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
    map = new_map;

    loadMap(this, nullptr);
}

void
TerrainNode::loadMap(const TerrainSettings* settings, IOControl* ioc)
{
    // remove everything and start over
    this->children.clear();

    _tilesRoot = vsg::Group::create();

    // Build the first level of the terrain.
    // Collect the tile keys comprising the root tiles of the terrain.
    //auto map = _map.lock();
    ROCKY_SOFT_ASSERT_AND_RETURN(map, void());

    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(settings->firstLOD, map->getProfile(), keys);

    // Now that we have a profile, set up the selection info
    selectionInfo.initialize(firstLOD, maxLOD, map->getProfile(), minTileRangeFactor, true);

#if 0
    // Load all the root key tiles.
    JobGroup loadGroup;
    Job load;
    load.setArena(ARENA_LOAD_TILE);
    load.setGroup(&loadGroup);
#endif

    for (unsigned i = 0; i < keys.size(); ++i)
    {
        auto tileNode = TerrainTileNode::create(
            keys[i],
            nullptr, // parent tile
            *this,   // terrain context
            ioc);

        tileNode->setDoNotExpire(true);

        // Add it to the scene graph
        _tilesRoot->addChild(tileNode);

#if 0 // temp
        // Post-add initialization:
        tileNode->initializeData(*this);
#endif 

#if 0
        // And load the tile's data
        load.dispatch([&](Cancelable*) {
            tileNode->loadSync(*this);
            });
#endif

        ROCKY_DEBUG << " - " << (i + 1) << "/" << keys.size() << " : " << keys[i].str() << std::endl;
    }

#if 0
    // wait for all loadSync calls to complete
    loadGroup.join();
#endif

    // create the graphics pipeline to render this map
    auto stateGroup = stateFactory.createTerrainStateGroup();
    stateGroup->addChild(_tilesRoot);
    this->addChild(stateGroup);
}

void
TerrainNode::traverse(vsg::RecordTraversal& nv) const
{
    // Pass the context along in the traversal
    nv.setObject("TerrainContext",
        static_cast<vsg::Object*>(
            const_cast<TerrainNode*>(this)));

    vsg::Group::traverse(nv);
}

void
TerrainNode::update(const vsg::FrameStamp* fs)
{
    tiles.update(*this, fs);
    //merger.update(*this);
    //unloader.update(*this);
}
