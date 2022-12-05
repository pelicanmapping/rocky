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

        ROCKY_SOFT_ASSERT_AND_RETURN(tileNode, void());

        // Next, build the surface geometry for the node.
        //tileNode->create( keys[i], 0L, _engineContext.get(), nullptr );
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
    auto pipeline = createPipeline(_tilesRoot);
    this->addChild(pipeline);
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

vsg::ref_ptr<vsg::Node>
TerrainNode::createPipeline(
    vsg::ref_ptr<vsg::Node> content) const
{
    // This method uses the "terrainShaderSet" as a prototype to
    // define a graphics pipeline that will render the terrain.

    auto gp_config = vsg::GraphicsPipelineConfig::create(
        terrainShaderSet);

    // activate the arrays we intend to use
    gp_config->enableArray("inVertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gp_config->enableArray("inNormal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gp_config->enableArray("inUV", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gp_config->enableArray("inNeighborVertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gp_config->enableArray("inNeighborNormal", VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // wireframe rendering:
    gp_config->rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;

    // Configure the descriptors we need for textures and uniforms:
    vsg::Descriptors descriptors;

#if 0 // TODO - one per layer...?
    if (stateInfo.image)
    {
        auto sampler = Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        if (sharedObjects) sharedObjects->share(sampler);

        graphicsPipelineConfig->assignTexture(descriptors, "diffuseMap", stateInfo.image, sampler);

        if (stateInfo.greyscale) defines.insert("VSG_GREYSACLE_DIFFUSE_MAP");
    }
#endif

    // Initialize GraphicsPipeline from the data in the configuration.
    if (sharedObjects)
        sharedObjects->share(gp_config, [](auto gpc) { gpc->init(); });
    else
        gp_config->init();

    // create StateGroup as the root of the scene/command graph to hold
    // the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();
    stateGroup->add(gp_config->bindGraphicsPipeline);

    // set up any samplers and uniforms the terrain intends to use;
    // these go in a descriptor set.
    if (!descriptors.empty())
    {
        // the descriptor set itself (coupled with its layout):
        auto descriptorSet = vsg::DescriptorSet::create(
            gp_config->descriptorSetLayout,
            descriptors);

        if (sharedObjects)
            sharedObjects->share(descriptorSet);

        // the bind command, which will run as part of the state group
        auto bindDescriptorSet = vsg::BindDescriptorSet::create(
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gp_config->layout,
            0,
            descriptorSet);

        if (sharedObjects)
            sharedObjects->share(bindDescriptorSet);

        stateGroup->add(bindDescriptorSet);

#if 0
        // the bind-view command, which binds descriptors for view-dependent
        // things like lights. Not sure if this goes here.
        auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            gp_config->layout,
            1);

        if (sharedObjects)
            sharedObjects->share(bindViewDescriptorSets);

        stateGroup->add(bindViewDescriptorSets);
#endif
    }

    // assign any custom ArrayState that may be required.
    stateGroup->prototypeArrayState = terrainShaderSet->getSuitableArrayState(
        gp_config->shaderHints->defines);

    stateGroup->addChild(content);

    return stateGroup;
}
