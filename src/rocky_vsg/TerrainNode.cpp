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


TerrainNode::TerrainNode() :
    vsg::Inherit<vsg::Group,TerrainNode>(),
    TerrainContext(Config())
{
    construct(Config());
}

TerrainNode::TerrainNode(const Config& conf) :
    vsg::Inherit<vsg::Group, TerrainNode>(),
    TerrainContext(conf)
{
    construct(conf);
}

void
TerrainNode::construct(const Config& conf)
{
    firstLOD.setDefault(0u);

    conf.get("first_lod", firstLOD);
}

Config
TerrainNode::getConfig() const
{
    Config conf("terrain");
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

    //this->addChild(_tilesRoot);

    // Build the first level of the terrain.
    // Collect the tile keys comprising the root tiles of the terrain.
    //auto map = _map.lock();
    ROCKY_SOFT_ASSERT_AND_RETURN(map, void());

    std::vector<TileKey> keys;
    Profile::getAllKeysAtLOD(settings->firstLOD, map->getProfile(), keys);

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
    const uint32_t mipmapLevelsHint = 16;
    const uint32_t reverseDepth = 0;
    const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    auto dataDirPath = vsg::Path(); // VSGEARTH_FULL_DATA_DIR);
    searchPaths.push_back(dataDirPath);
    searchPaths.push_back(vsg::Path("H:/devel/rocky/install/share"));

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vsg::findFile("terrain.vert.spv",
            searchPaths));

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile("terrain.frag.spv",
        searchPaths));

    if (!vertexShader || !fragmentShader)
    {
        ROCKY_WARN << "Could not create shaders." << std::endl;
        return content;
        //return vsg::ref_ptr<vsg::Node>(nullptr);
    }

    vsg::DescriptorSetLayoutBindings layerDescriptorBindings {
        // layer parameters
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    };
    auto layerDescriptorSetLayout = vsg::DescriptorSetLayout::create(layerDescriptorBindings);

    // Now the visible layers
    auto layers = map->getLayers<ImageLayer>([&](const ImageLayer* layer) {
        return layer && layer->isOpen();
        });

    uint32_t numLayers = (uint32_t)layers.size();

#if 0
    auto layerParams = LayerParams::create(numLayers);
    for (int i = 0; i < numImageLayers; ++i)
    {
        layerParams->layerUIDs[i] = imageLayers[i]->getUID();
        layerParams->setEnabled(i, imageLayers[i]->getVisible());
        layerParams->setOpacity(i, imageLayers[i]->getOpacity());
        layerParams->setBlendMode(i, imageLayers[i]->getColorBlending());
        imageLayers[i]->Layer::addCallback(new VOELayerCallback(layerParams, i));
    }
#endif

    vsg::DescriptorSetLayoutBindings descriptorSetBindings
    {
        // color layers:
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numLayers, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},

        // elevation:
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},

        // normal:
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},

        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorSetBindings);

    // projection view, and model matrices, actual push constant calls autoaatically provided by
    // VSG's DispatchTraversal
    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} 
    };

    auto pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{
            descriptorSetLayout,
            vsg::ViewDescriptorSetLayout::create(),
            layerDescriptorSetLayout },
        pushConstantRanges);

    auto sampler = vsg::Sampler::create();
    sampler->maxLod = mipmapLevelsHint;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->anisotropyEnable = VK_TRUE;
    sampler->maxAnisotropy = 16.0f;

    auto elevationSampler = vsg::Sampler::create();
    elevationSampler->maxLod = 0;
    elevationSampler->minFilter = VK_FILTER_NEAREST;
    elevationSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    elevationSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    elevationSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    auto normalSampler = vsg::Sampler::create();
    normalSampler->maxLod = 0;
    normalSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    normalSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    normalSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

#if 0
    osg::Texture* emptyOsgElevation = createEmptyElevationTexture();
    auto emptyElevData = convertToVsg(emptyOsgElevation->getImage(0));
    emptyElevationDescImage = vsg::DescriptorImage::create(
        elevationSampler,
        emptyElevData,
        1,
        0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    osg::Texture* emptyOsgNormal = createEmptyNormalMapTexture();
    auto emptyNormalData = convertToVsg(emptyOsgNormal->getImage(0));
    emptyNormalDescImage = vsg::DescriptorImage::create(
        normalSampler,
        emptyNormalData,
        2,
        0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
#endif

    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // verts
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normals
        VkVertexInputBindingDescription{2, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // uvs
        VkVertexInputBindingDescription{3, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // neighbors
        VkVertexInputBindingDescription{4, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // neighbor normals
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // verts
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normals
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0}, // uvs
        VkVertexInputAttributeDescription{3, 3, VK_FORMAT_R32G32B32_SFLOAT, 0}, // neighbors
        VkVertexInputAttributeDescription{3, 3, VK_FORMAT_R32G32B32_SFLOAT, 0}  // neighbor normals
    };

    auto depthStencilState = vsg::DepthStencilState::create();

    vsg::ShaderStage::SpecializationConstants specializationConstants
    {
        {0, vsg::uintValue::create(reverseDepth)},
        {1, vsg::uintValue::create(numLayers)}
    };
    vertexShader->specializationConstants = specializationConstants;
    fragmentShader->specializationConstants = specializationConstants;

    vsg::GraphicsPipelineStates fillPipelineStates
    {
        vsg::RasterizationState::create(),
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::MultisampleState::create(samples),
        vsg::ColorBlendState::create(),
        depthStencilState
    };

    auto wireRasterState = vsg::RasterizationState::create();
    wireRasterState->polygonMode = VK_POLYGON_MODE_LINE;
    vsg::GraphicsPipelineStates wirePipelineStates(fillPipelineStates);
    wirePipelineStates[0] = wireRasterState;

    auto pointRasterState = vsg::RasterizationState::create();
    pointRasterState->polygonMode = VK_POLYGON_MODE_POINT;
    vsg::GraphicsPipelineStates pointPipelineStates(fillPipelineStates);
    pointPipelineStates[0] = pointRasterState;

    vsg::ShaderStages shaderStages { vertexShader, fragmentShader };

    auto lightStateGroup = vsg::StateGroup::create();

    lightStateGroup->add(vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1));

#if 0
    auto layerDescriptorSet = vsg::DescriptorSet::create(
        layerDescriptorSetLayout,
        vsg::Descriptors{ layerParams->layerParamsDescriptor });

    auto bindLayerDescriptorSet = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        2,
        layerDescriptorSet);

    lightStateGroup->add(bindLayerDescriptorSet);
#endif

    // StateSwitch experiments
    auto rasterStateGroup = vsg::StateGroup::create();
    auto rasterSwitch = vsg::StateSwitch::create();

    vsg::GraphicsPipelineStates* pipelineStates[] = {
        &fillPipelineStates,
        &wirePipelineStates,
        &pointPipelineStates };

    for (int i = 0; i < 3; ++i)
    {
        auto graphicsPipeline = vsg::GraphicsPipeline::create(
            pipelineLayout,
            shaderStages,
            *pipelineStates[i]);

        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);
        rasterSwitch->add(i == 0, bindGraphicsPipeline);
    }
    rasterStateGroup->add(rasterSwitch);
    rasterStateGroup->addChild(lightStateGroup);

    //auto plodRoot = vsg::read_cast<vsg::Node>("root.tile", options);

    lightStateGroup->addChild(content);

    // Put everything under a group node in order to have a place to hang the lights.

#if 0
    auto sceneGroup = vsg::Group::create();

    sceneGroup->addChild(rasterStateGroup);
    directionalLight = vsg::DirectionalLight::create();
    directionalLight->name = "directional";
    directionalLight->color = simState.getColor();
    directionalLight->intensity = 1.0f;
    directionalLight->direction = simState.worldDirection;
    sceneGroup->addChild(directionalLight);
    ambientLight = vsg::AmbientLight::create();
    ambientLight->name = "ambient";
    ambientLight->color = simState.getAmbient();
    ambientLight->intensity = 1.0f;
    sceneGroup->addChild(ambientLight);
    // assign the EllipsoidModel so that the overall geometry of the database can be used as guide
    // for clipping and navigation.
    sceneGroup->setObject("EllipsoidModel", ellipsoidModel);
    return sceneGroup;
#endif

    return rasterStateGroup;
}
