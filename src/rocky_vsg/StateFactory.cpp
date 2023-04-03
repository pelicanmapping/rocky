/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "StateFactory.h"
#include "RuntimeContext.h"
#include "TerrainTileNode.h"

#include <rocky/Color.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>
#include <rocky_vsg/Utils.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>

#define TERRAIN_VERT_SHADER "rocky.terrain.vert"
#define TERRAIN_FRAG_SHADER "rocky.terrain.frag"

#define ELEVATION_TEX_NAME "elevation_tex"
#define ELEVATION_TEX_BINDING 10

#define COLOR_TEX_NAME "color_tex"
#define COLOR_TEX_BINDING 11

#define NORMAL_TEX_NAME "normal_tex"
#define NORMAL_TEX_BINDING 12

#define TILE_BUFFER_NAME "terrain_tile"
#define TILE_BUFFER_BINDING 13

#define LIGHT_DATA "vsg_lights"

#define ATTR_VERTEX "in_vertex"
#define ATTR_NORMAL "in_normal"
#define ATTR_UV "in_uvw"
#define ATTR_VERTEX_NEIGHBOR "in_vertex_neighbor"
#define ATTR_NORMAL_NEIGHBOR "in_normal_neighbor"

using namespace ROCKY_NAMESPACE;

StateFactory::StateFactory()
{
    status = StatusOK;

    // cache of shared objects in the terrain state. Re-using objects
    // helps performance.
    sharedObjects = vsg::SharedObjects::create();

    // set up the texture samplers and placeholder images we will use to render terrain.
    createDefaultDescriptors();

    // shader set prototype for use with a GraphicsPipelineConfig.
    shaderSet = createShaderSet();
    if (!shaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Terrain shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share/shaders folder?");
        return;
    }

    // create the pipeline configurator for terrain; this is a helper object
    // that acts as a "template" for terrain tile rendering state.
    pipelineConfig = createPipelineConfig(sharedObjects.get());

    //ALT:
    //pipeline = createPipeline(sharedObjects.get());
}

void
StateFactory::createDefaultDescriptors()
{
    // First create our samplers - each one is shared across all tiles.
    // In Vulkan, the sampler is separate from the image you are sampling,
    // so you can share a sampler between any number of images.

    // color channel
    // TODO: more than one - make this an array?
    // TODO: activate mipmapping
    textures.color = { COLOR_TEX_NAME, COLOR_TEX_BINDING, vsg::Sampler::create(), {} };
    textures.color.sampler->minFilter = VK_FILTER_LINEAR;
    textures.color.sampler->magFilter = VK_FILTER_LINEAR;
    textures.color.sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    textures.color.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.color.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.color.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.color.sampler->anisotropyEnable = VK_TRUE;
    textures.color.sampler->maxAnisotropy = 4.0f;
    if (sharedObjects) sharedObjects->share(textures.color.sampler);

    textures.elevation = { ELEVATION_TEX_NAME, ELEVATION_TEX_BINDING, vsg::Sampler::create(), {} };
    textures.elevation.sampler->maxLod = 16;
    textures.elevation.sampler->minFilter = VK_FILTER_LINEAR;
    textures.elevation.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.elevation.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.elevation.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (sharedObjects) sharedObjects->share(textures.elevation.sampler);

    textures.normal = { NORMAL_TEX_NAME, NORMAL_TEX_BINDING, vsg::Sampler::create(), {} };
    textures.normal.sampler->maxLod = 16;
    textures.normal.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.normal.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.normal.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (sharedObjects) sharedObjects->share(textures.normal.sampler);


    // Next make the "default" descriptor model, which is used when 
    // no other data is available. These are 1x1 pixel placeholder images.
    auto color_image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
    color_image->write(Color::White, 0, 0);
    textures.color.defaultData = util::moveImageToVSG(color_image);
    ROCKY_HARD_ASSERT(textures.color.defaultData);
    this->defaultTileDescriptors.color = vsg::DescriptorImage::create(
        textures.color.sampler,
        textures.color.defaultData,
        textures.color.uniform_binding,
        0, // array element (TODO: increment when we change to an array)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto elev_image = Heightfield::create(1, 1);
    elev_image->fill(0.0f);
    textures.elevation.defaultData = util::moveImageToVSG(elev_image);
    ROCKY_HARD_ASSERT(textures.elevation.defaultData);
    this->defaultTileDescriptors.elevation = vsg::DescriptorImage::create(
        textures.elevation.sampler,
        textures.elevation.defaultData,
        textures.elevation.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto normal_image = Image::create(Image::R8G8B8_UNORM, 1, 1);
    normal_image->fill(fvec4(.5, .5, 1, 0));
    textures.normal.defaultData = util::moveImageToVSG(normal_image);
    ROCKY_HARD_ASSERT(textures.normal.defaultData);
    this->defaultTileDescriptors.normal = vsg::DescriptorImage::create(
        textures.normal.sampler,
        textures.normal.defaultData,
        textures.normal.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

vsg::ref_ptr<vsg::ShaderSet>
StateFactory::createShaderSet() const
{
    // Creates a ShaderSet for terrain rendering.
    //
    // A ShaderSet is a combination of shader stages (vert, frag),
    // attribute bindings (vertex, normal, etc), uniform bindings,
    // and push constants -- basically everything you will access in
    // the shaders.
    //
    // Later, you can use a GraphicsPipelineConfig to make a GraphicsPipeline
    // that "customizes" the ShaderSet by enabling just the attributes, uniforms,
    // textures etc. that you need and using defines to figure it all out.
    // This is the basis of the VSG state composition setup.

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    vsg::Paths morePaths = vsg::getEnvPaths("ROCKY_FILE_PATH");
    searchPaths.insert(searchPaths.end(), morePaths.begin(), morePaths.end());

    auto options = vsg::Options::create();

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vsg::findFile(TERRAIN_VERT_SHADER, searchPaths),
        options);

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile(TERRAIN_FRAG_SHADER, searchPaths),
        options);

    if (!vertexShader || !fragmentShader)
    {
        return { };
    }

#if 0
    const uint32_t reverseDepth = 0;
    const uint32_t numLayers = 0;
    vsg::ShaderStage::SpecializationConstants specializationConstants
    {
        {0, vsg::uintValue::create(reverseDepth)},
        {1, vsg::uintValue::create(numLayers)}
    };
    vertexShader->specializationConstants = specializationConstants;
    fragmentShader->specializationConstants = specializationConstants;
#endif

    vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

    shaderSet = vsg::ShaderSet::create(shaderStages);

    // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
    shaderSet->addAttributeBinding(ATTR_VERTEX, "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_NORMAL, "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_UV, "", 2, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_VERTEX_NEIGHBOR, "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_NORMAL_NEIGHBOR, "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    // "binding" (4th param) must match "layout(location=X) uniform" in the shader
    shaderSet->addUniformBinding(textures.elevation.name, "", 0, textures.elevation.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, {}); // , vsg::floatArray2D::create(1, 1));
    shaderSet->addUniformBinding(textures.color.name, "", 0, textures.color.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {}); // , vsg::vec3Array2D::create(1, 1));
    shaderSet->addUniformBinding(textures.normal.name, "", 0, textures.normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {}); // , vsg::vec3Array2D::create(1, 1));

    shaderSet->addUniformBinding(TILE_BUFFER_NAME, "", 0, TILE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {}); // , vsg::vec3Array2D::create(1, 1));

    shaderSet->addUniformBinding("vsg_lights", "", 1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));

    // Note: 128 is the maximum size required by the Vulkan spec, 
    // so don't increase it :)
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    return shaderSet;
}


vsg::ref_ptr<vsg::GraphicsPipelineConfig>
StateFactory::createPipelineConfig(vsg::SharedObjects* sharedObjects) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), {});

    // This method uses the "shaderSet" as a prototype to
    // define a graphics pipeline that will render the terrain.

    auto pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

    // activate the arrays we intend to use
    pipelineConfig->enableArray(ATTR_VERTEX, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray(ATTR_NORMAL, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray(ATTR_UV, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray(ATTR_VERTEX_NEIGHBOR, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray(ATTR_NORMAL_NEIGHBOR, VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // wireframe rendering:
    //pipelineConfig->rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;

    // backface culling off:
    //pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_NONE;

    // Temporary decriptors that we will use to set up the PipelineConfig.
    // Note, we only use these for setup, and then throw them away!
    // The ACTUAL descriptors we will make on a tile-by-tile basis.
    vsg::Descriptors descriptors;
    pipelineConfig->assignTexture(descriptors, textures.elevation.name, textures.elevation.defaultData, textures.elevation.sampler);
    pipelineConfig->assignTexture(descriptors, textures.color.name, textures.color.defaultData, textures.color.sampler);
    pipelineConfig->assignTexture(descriptors, textures.normal.name, textures.normal.defaultData, textures.normal.sampler);

    pipelineConfig->assignUniform(descriptors, TILE_BUFFER_NAME, { });

    if (auto& lightDataBinding = shaderSet->getUniformBinding("vsg_lights"))
    {
        auto data = lightDataBinding.data;
        if (!data) data = vsg::vec4Array::create(64);
        pipelineConfig->assignUniform(descriptors, LIGHT_DATA, data);
    }

    // Register the ViewDescriptorSetLayout (for view-dependent state stuff
    // like viewpoint and lights data)
    // The "set" in GLSL's "layout(set=X, binding=Y)" refers to the index of
    // the descriptor set layout within the pipeline layout. Setting the
    // "additional" DSL appends it to the pipline layout, giving it set=1.
    vsg::ref_ptr<vsg::ViewDescriptorSetLayout> vdsl;
    if (sharedObjects)
        vdsl = sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>();
    else
        vdsl = vsg::ViewDescriptorSetLayout::create();

    pipelineConfig->additionalDescriptorSetLayout = vdsl;

    // Initialize GraphicsPipeline from the data in the configuration.
    if (sharedObjects)
        sharedObjects->share(pipelineConfig, [](auto gpc) { gpc->init(); });
    else
        pipelineConfig->init();

    return pipelineConfig;
}

#if 0
vsg::ref_ptr<vsg::GraphicsPipeline>
StateFactory::createPipeline(vsg::SharedObjects* sharedObjects) const
{
    // Hello. This is the "manual path", which is an alternative to using
    // the GraphicsPipelineConfig.
    // We probably want the latter moving forward so we can use the 
    // shader/state composition capability, but this remains here for reference.


    //vsg::DescriptorSetLayoutBindings layerDescriptorBindings
    //{
    //    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    //};
    //auto layerDescriptorSetLayout = vsg::DescriptorSetLayout::create(layerDescriptorBindings);

    const uint32_t numColorLayers = 1u;
    const VkSampleCountFlagBits sampleBits = VK_SAMPLE_COUNT_1_BIT;

    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {textures.elevation.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {textures.color.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numColorLayers, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {textures.normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {TILE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    
        //{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }
    };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges
    {
        // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
    }; 
    
    auto pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts
        {
            descriptorSetLayout,
            vsg::ViewDescriptorSetLayout::create()
            //, layerDescriptorSetLayout // TODO?
        },
        pushConstantRanges
    );

    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normal
        VkVertexInputBindingDescription{2, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // uv
        VkVertexInputBindingDescription{3, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // neighbor vertex
        VkVertexInputBindingDescription{4, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // neighbor normal
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normal
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0}, // uv
        VkVertexInputAttributeDescription{3, 3, VK_FORMAT_R32G32B32_SFLOAT, 0}, // neighbor vertex
        VkVertexInputAttributeDescription{4, 4, VK_FORMAT_R32G32B32_SFLOAT, 0}, // neighbor normal
    };

    //vsg::ShaderStage::SpecializationConstants specializationConstants{
    //    {0, vsg::uintValue::create(reverseDepth)},
    //    {1, vsg::uintValue::create(numImageLayers)}
    //};
    //vertexShader->specializationConstants = specializationConstants;
    //fragmentShader->specializationConstants = specializationConstants;

    vsg::GraphicsPipelineStates fillPipelineStates
    {
        vsg::RasterizationState::create(),
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::MultisampleState::create(sampleBits),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    return vsg::GraphicsPipeline::create(
        pipelineLayout,
        shaderSet->stages,
        fillPipelineStates);
}
#endif

vsg::ref_ptr<vsg::StateGroup>
StateFactory::createTerrainStateGroup() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), { });

    // Just a StateGroup holding the graphics pipeline.
    // No actual descriptors here - those will appear on each tile.

    auto stateGroup = vsg::StateGroup::create();

    if (pipelineConfig)
        stateGroup->add(pipelineConfig->bindGraphicsPipeline);
    else if (pipeline)
        stateGroup->add(vsg::BindGraphicsPipeline::create(pipeline));

    // assign any custom ArrayState that may be required.
    stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(pipelineConfig->shaderHints->defines);

    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, 1);

    if (sharedObjects) sharedObjects->share(bindViewDescriptorSets);
    stateGroup->add(bindViewDescriptorSets);

    return stateGroup;
}

void
StateFactory::updateTerrainTileDescriptors(
    const TerrainTileRenderModel& renderModel,
    vsg::ref_ptr<vsg::StateGroup> stategroup,
    RuntimeContext& runtime) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), void());

    // Takes a tile's render model (which holds the raw image and matrix data)
    // and creates the necessary VK data to render that model.

    // copy the existing one:
    TerrainTileDescriptors dm = renderModel.descriptors;

    if (renderModel.color.image)
    {
        auto data = util::moveImageToVSG(renderModel.color.image->clone());
        if (data)
        {
            dm.color = vsg::DescriptorImage::create(
                textures.color.sampler,
                data,
                textures.color.uniform_binding,
                0, // array element (TODO: increment if we change to an array)
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    if (renderModel.elevation.image)
    {
        auto data = util::moveImageToVSG(renderModel.elevation.image->clone());
        if (data)
        {
            dm.elevation = vsg::DescriptorImage::create(
                textures.elevation.sampler,
                data,
                textures.elevation.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    if (renderModel.normal.image)
    {
        auto data = util::moveImageToVSG(renderModel.normal.image->clone());
        if (data)
        {
            dm.normal = vsg::DescriptorImage::create(
                textures.normal.sampler,
                data,
                textures.normal.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    TerrainTileDescriptors::Uniforms uniforms;
    uniforms.elevation_matrix = renderModel.elevation.matrix;
    uniforms.color_matrix = renderModel.color.matrix;
    uniforms.normal_matrix = renderModel.normal.matrix;
    uniforms.model_matrix = renderModel.modelMatrix;

    vsg::ref_ptr<vsg::ubyteArray> data = vsg::ubyteArray::create(sizeof(uniforms));
    memcpy(data->dataPointer(), &uniforms, sizeof(uniforms));
    dm.uniforms = vsg::DescriptorBuffer::create(
        data,
        TILE_BUFFER_BINDING);

    // we can get rid of this once we decide finally on config versus pipeline approach
    auto pipelineLayout =
        pipeline ? pipeline->layout :
        pipelineConfig->layout;
        
    auto descriptorSetLayout = pipelineLayout->setLayouts.front();

    auto descriptorSet = vsg::DescriptorSet::create(
        descriptorSetLayout,
        vsg::Descriptors{ dm.elevation, dm.color, dm.normal, dm.uniforms }
    );
    //if (sharedObjects) sharedObjects->share(descriptorSet);

    dm.bindDescriptorSetCommand = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, // first set
        descriptorSet
    );

    // No...?
    //if (sharedObjects) sharedObjects->share(dm.bindDescriptorSetCommand);

    if (stategroup)
    {
        // Need to compile the descriptors
        if (runtime.compiler)
        {
            runtime.compiler()->compile(dm.bindDescriptorSetCommand);
        }

        // And update the tile's state group
        stategroup->stateCommands.clear();
        stategroup->add(dm.bindDescriptorSetCommand);
    }
}
