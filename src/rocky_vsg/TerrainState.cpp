/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainState.h"
#include "Runtime.h"
#include "TerrainTileNode.h"

#include <rocky/Color.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>
#include <rocky_vsg/Utils.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>

#define TERRAIN_VERT_SHADER "shaders/rocky.terrain.vert"
#define TERRAIN_FRAG_SHADER "shaders/rocky.terrain.frag"

#define ELEVATION_TEX_NAME "elevation_tex"
#define ELEVATION_TEX_BINDING 10

#define COLOR_TEX_NAME "color_tex"
#define COLOR_TEX_BINDING 11

#define NORMAL_TEX_NAME "normal_tex"
#define NORMAL_TEX_BINDING 12

#define TILE_BUFFER_NAME "tile"
#define TILE_BUFFER_BINDING 13

#define LIGHTS_BUFFER_NAME "vsg_lights"
#define LIGHTS_BUFFER_SET 1
#define LIGHTS_BUFFER_BINDING 0

#define ATTR_VERTEX "in_vertex"
#define ATTR_NORMAL "in_normal"
#define ATTR_UV "in_uvw"
#define ATTR_VERTEX_NEIGHBOR "in_vertex_neighbor"
#define ATTR_NORMAL_NEIGHBOR "in_normal_neighbor"

using namespace ROCKY_NAMESPACE;

TerrainState::TerrainState(Runtime& runtime) :
    _runtime(runtime)
{
    status = StatusOK;

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
}

void
TerrainState::createDefaultDescriptors()
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
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(textures.color.sampler);

    textures.elevation = { ELEVATION_TEX_NAME, ELEVATION_TEX_BINDING, vsg::Sampler::create(), {} };
    textures.elevation.sampler->maxLod = 16;
    textures.elevation.sampler->minFilter = VK_FILTER_LINEAR;
    textures.elevation.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.elevation.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.elevation.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(textures.elevation.sampler);

    textures.normal = { NORMAL_TEX_NAME, NORMAL_TEX_BINDING, vsg::Sampler::create(), {} };
    textures.normal.sampler->maxLod = 16;
    textures.normal.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.normal.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    textures.normal.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(textures.normal.sampler);


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
TerrainState::createShaderSet() const
{
    // Creates a ShaderSet for terrain rendering.
    //
    // A ShaderSet is a combination of shader stages (vert, frag),
    // attribute bindings (vertex, normal, etc), uniform bindings,
    // and push constants -- basically everything you will access in
    // the shaders.
    //
    // One you have the ShaderSet you can use a GraphicsPipelineConfig to make a
    // GraphicsPipeline that "customizes" the ShaderSet by enabling just the
    // attributes, uniforms, textures etc. that you need and using defines to
    // figure it all out. This is the basis of the VSG state composition setup.

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vsg::findFile(TERRAIN_VERT_SHADER, _runtime.searchPaths),
        _runtime.readerWriterOptions);

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile(TERRAIN_FRAG_SHADER, _runtime.searchPaths),
        _runtime.readerWriterOptions);

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
    //shaderSet->addAttributeBinding(ATTR_VERTEX_NEIGHBOR, "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding(ATTR_NORMAL_NEIGHBOR, "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));

    // "binding" (4th param) must match "layout(location=X) uniform" in the shader
    shaderSet->addUniformBinding(textures.elevation.name, "", 0, textures.elevation.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});
    shaderSet->addUniformBinding(textures.color.name, "", 0, textures.color.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addUniformBinding(textures.normal.name, "", 0, textures.normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addUniformBinding(TILE_BUFFER_NAME, "", 0, TILE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addUniformBinding(LIGHTS_BUFFER_NAME, "", LIGHTS_BUFFER_SET, LIGHTS_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));

    // Note: 128 is the maximum size required by the Vulkan spec, 
    // so don't increase it :)
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    return shaderSet;
}


vsg::ref_ptr<vsg::GraphicsPipelineConfig>
TerrainState::createPipelineConfig() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), {});

    // Create the pipeline configurator for terrain; this is a helper object
    // that acts as a "template" for terrain tile rendering state.
    auto config = vsg::GraphicsPipelineConfig::create(shaderSet);

    // Apply any custom compile settings / defines:
    config->shaderHints = _runtime.shaderCompileSettings;

    // activate the arrays we intend to use
    config->enableArray(ATTR_VERTEX, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    config->enableArray(ATTR_NORMAL, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    config->enableArray(ATTR_UV, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    //config->enableArray(ATTR_VERTEX_NEIGHBOR, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    //config->enableArray(ATTR_NORMAL_NEIGHBOR, VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // wireframe rendering:
    //config->rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;

    // backface culling off:
    //config->rasterizationState->cullMode = VK_CULL_MODE_NONE;

    // Temporary decriptors that we will use to set up the PipelineConfig.
    // Note, we only use these for setup, and then throw them away!
    // The ACTUAL descriptors we will make on a tile-by-tile basis.
    vsg::Descriptors descriptors;
    config->assignTexture(descriptors, textures.elevation.name, textures.elevation.defaultData, textures.elevation.sampler);
    config->assignTexture(descriptors, textures.color.name, textures.color.defaultData, textures.color.sampler);
    config->assignTexture(descriptors, textures.normal.name, textures.normal.defaultData, textures.normal.sampler);
    config->assignUniform(descriptors, TILE_BUFFER_NAME, { });
    config->assignUniform(descriptors, LIGHTS_BUFFER_NAME, { });

    // Register the ViewDescriptorSetLayout (for view-dependent state stuff
    // like viewpoint and lights data)
    // The "set" in GLSL's "layout(set=X, binding=Y)" refers to the index of
    // the descriptor set layout within the pipeline layout. Setting the
    // "additional" DSL appends it to the pipline layout, giving it set=1.
    config->additionalDescriptorSetLayout =
        _runtime.sharedObjects ? _runtime.sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>() :
        vsg::ViewDescriptorSetLayout::create();

    // Initialize GraphicsPipeline from the data in the configuration.
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(config, [](auto gpc) { gpc->init(); });
    else
        config->init();

    return config;
}

vsg::ref_ptr<vsg::StateGroup>
TerrainState::createTerrainStateGroup()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), { });

    // create the configurator object:
    pipelineConfig = createPipelineConfig();

    ROCKY_SOFT_ASSERT_AND_RETURN(pipelineConfig, { });

    // Just a StateGroup holding the graphics pipeline.
    // No actual descriptors here - those will appear on each tile.
    // (except for the view-dependent state stuff from VSG)
    auto stateGroup = vsg::StateGroup::create();
    stateGroup->add(pipelineConfig->bindGraphicsPipeline);

    // This binds the view-dependent state from VSG (lights, viewport, etc.)
    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, 1);
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(bindViewDescriptorSets);
    stateGroup->add(bindViewDescriptorSets);

    return stateGroup;
}

void
TerrainState::updateTerrainTileDescriptors(
    const TerrainTileRenderModel& renderModel,
    vsg::ref_ptr<vsg::StateGroup> stategroup,
    Runtime& runtime) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(pipelineConfig.valid(), void());

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

    // the per-tile uniform block:
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

    // the samplers:
    auto descriptorSetLayout = pipelineConfig->layout->setLayouts.front();

    auto descriptorSet = vsg::DescriptorSet::create(
        descriptorSetLayout,
        vsg::Descriptors{ dm.elevation, dm.color, dm.normal, dm.uniforms }
    );
    //if (sharedObjects) sharedObjects->share(descriptorSet);

    dm.bindDescriptorSetCommand = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineConfig->layout,
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
