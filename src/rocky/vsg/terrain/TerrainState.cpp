/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainState.h"
#include "TerrainNode.h"
#include "TerrainTileNode.h"
#include "../VSGUtils.h"
#include "../PipelineState.h"

#include <rocky/Color.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>
#include <rocky/TerrainTileModel.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>

#define TERRAIN_VERT_SHADER "shaders/rocky.terrain.vert"
#define TERRAIN_FRAG_SHADER "shaders/rocky.terrain.frag"

#define SETTINGS_UBO_NAME "settings"
#define SETTINGS_UBO_BINDING 9

#define ELEVATION_TEX_NAME "elevation_tex"
#define ELEVATION_TEX_BINDING 10

#define COLOR_TEX_NAME "color_tex"
#define COLOR_TEX_BINDING 11

#define NORMAL_TEX_NAME "normal_tex"
#define NORMAL_TEX_BINDING 12

#define TILE_UBO_NAME "tile"
#define TILE_UBO_BINDING 13

#define ATTR_VERTEX "in_vertex"
#define ATTR_NORMAL "in_normal"
#define ATTR_UV "in_uvw"
#define ATTR_VERTEX_NEIGHBOR "in_vertex_neighbor"
#define ATTR_NORMAL_NEIGHBOR "in_normal_neighbor"

using namespace ROCKY_NAMESPACE;

TerrainState::TerrainState(VSGContext context)
{
    // set up the texture samplers and placeholder images we will use to render terrain.
    createDefaultDescriptors(context);

    // shader set prototype for use with a GraphicsPipelineConfig.
    shaderSet = createShaderSet(context);
    if (!shaderSet)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Terrain shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }
}

void
TerrainState::createDefaultDescriptors(VSGContext& context)
{
    // First create our samplers - each one is shared across all tiles.
    // In Vulkan, the sampler is separate from the image you are sampling,
    // so you can share a sampler between any number of images.

    // color channel
    // TODO: more than one - make this an array?
    // TODO: activate mipmapping
    texturedefs.color = { COLOR_TEX_NAME, COLOR_TEX_BINDING, vsg::Sampler::create(), {} };
    texturedefs.color.sampler->minFilter = VK_FILTER_LINEAR;
    texturedefs.color.sampler->magFilter = VK_FILTER_LINEAR;
    texturedefs.color.sampler->maxLod = 5;
    texturedefs.color.sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    texturedefs.color.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.color.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.color.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.color.sampler->anisotropyEnable = VK_TRUE;
    texturedefs.color.sampler->maxAnisotropy = 4.0f;
    if (context->sharedObjects)
        context->sharedObjects->share(texturedefs.color.sampler);

    texturedefs.elevation = { ELEVATION_TEX_NAME, ELEVATION_TEX_BINDING, vsg::Sampler::create(), {} };
    texturedefs.elevation.sampler->maxLod = 16;
    texturedefs.elevation.sampler->minFilter = VK_FILTER_LINEAR;
    texturedefs.elevation.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.elevation.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.elevation.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (context->sharedObjects)
        context->sharedObjects->share(texturedefs.elevation.sampler);

#if 0
    texturedefs.normal = { NORMAL_TEX_NAME, NORMAL_TEX_BINDING, vsg::Sampler::create(), {} };
    texturedefs.normal.sampler->maxLod = 16;
    texturedefs.normal.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.normal.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.normal.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (context->sharedObjects)
        context->sharedObjects->share(texturedefs.normal.sampler);
#endif

    // Next make the "default" descriptor model, which is used when 
    // no other data is available. These are 1x1 pixel placeholder images.
    auto color_image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
    color_image->write(Color("#08AEE0"), 0, 0);
    texturedefs.color.defaultData = util::moveImageToVSG(color_image);
    ROCKY_HARD_ASSERT(texturedefs.color.defaultData);
    this->defaultTileDescriptors.color = vsg::DescriptorImage::create(
        texturedefs.color.sampler,
        texturedefs.color.defaultData,
        texturedefs.color.uniform_binding,
        0, // array element (TODO: increment when we change to an array)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto hf = Heightfield::create(1, 1);
    hf.fill(0.0f);
    texturedefs.elevation.defaultData = util::moveImageToVSG(hf.image);
    ROCKY_HARD_ASSERT(texturedefs.elevation.defaultData);
    this->defaultTileDescriptors.elevation = vsg::DescriptorImage::create(
        texturedefs.elevation.sampler,
        texturedefs.elevation.defaultData,
        texturedefs.elevation.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

#if 0
    auto normal_image = Image::create(Image::R8G8B8_UNORM, 1, 1);
    normal_image->fill(glm::fvec4(.5, .5, 1, 0));
    texturedefs.normal.defaultData = util::moveImageToVSG(normal_image);
    ROCKY_HARD_ASSERT(texturedefs.normal.defaultData);
    this->defaultTileDescriptors.normal = vsg::DescriptorImage::create(
        texturedefs.normal.sampler,
        texturedefs.normal.defaultData,
        texturedefs.normal.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
#endif

}

vsg::ref_ptr<vsg::ShaderSet>
TerrainState::createShaderSet(VSGContext& context) const
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
        vsg::findFile(TERRAIN_VERT_SHADER, context->searchPaths),
        context->readerWriterOptions);

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile(TERRAIN_FRAG_SHADER, context->searchPaths),
        context->readerWriterOptions);

    if (!vertexShader || !fragmentShader)
    {
        return { };
    }

    vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

    shaderSet = vsg::ShaderSet::create(shaderStages);

    // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
    shaderSet->addAttributeBinding(ATTR_VERTEX, "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_NORMAL, "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding(ATTR_UV, "", 2, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding(ATTR_VERTEX_NEIGHBOR, "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding(ATTR_NORMAL_NEIGHBOR, "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));

    // "binding" (4th param) must match "layout(location=X) uniform" in the shader
    shaderSet->addDescriptorBinding(texturedefs.elevation.name, "", 0, texturedefs.elevation.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});
    shaderSet->addDescriptorBinding(texturedefs.color.name, "", 0, texturedefs.color.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    //shaderSet->addDescriptorBinding(texturedefs.normal.name, "", 0, texturedefs.normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding(TILE_UBO_NAME, "", 0, TILE_UBO_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding(SETTINGS_UBO_NAME, "", 0, SETTINGS_UBO_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    
    PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_FRAGMENT_BIT);

    // Note: 128 is the maximum size required by the Vulkan spec, 
    // so don't increase it :)
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    return shaderSet;
}


vsg::ref_ptr<vsg::GraphicsPipelineConfig>
TerrainState::createPipelineConfig(VSGContext& context) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), {});

    // Create the pipeline configurator for terrain; this is a helper object
    // that acts as a "template" for terrain tile rendering state.
    auto config = vsg::GraphicsPipelineConfig::create(shaderSet);

    // Apply any custom compile settings / defines:
    config->shaderHints = context->shaderCompileSettings;

    // activate the arrays we intend to use
    config->enableArray(ATTR_VERTEX, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    config->enableArray(ATTR_NORMAL, VK_VERTEX_INPUT_RATE_VERTEX, 12);
    config->enableArray(ATTR_UV, VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // activate the descriptors we intend to use
    config->enableTexture(texturedefs.elevation.name);
    config->enableTexture(texturedefs.color.name);
    //config->enableTexture(texturedefs.normal.name);

    config->enableDescriptor(TILE_UBO_NAME);
    config->enableDescriptor(SETTINGS_UBO_NAME);

    PipelineUtils::enableViewDependentData(config);

    struct SetPipelineStates : public vsg::Visitor
    {
        void apply(vsg::Object& object) override {
            object.traverse(*this);
        }
        void apply(vsg::RasterizationState& state) override {
            state.cullMode = VK_CULL_MODE_BACK_BIT;
        }
        void apply(vsg::DepthStencilState& state) override {
        }
        void apply(vsg::ColorBlendState& state) override {
        }
    };
    SetPipelineStates visitor;
    config->accept(visitor);

    config->init();

    return config;
}

bool
TerrainState::setupTerrainStateGroup(vsg::StateGroup& stateGroup, VSGContext& context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), false);

    // create the configurator object:
    pipelineConfig = createPipelineConfig(context);

    ROCKY_SOFT_ASSERT_AND_RETURN(pipelineConfig, false);

    if (!_terrainDescriptors.data)
    {
        // global settings uniform setup
        _terrainDescriptors.data = vsg::ubyteArray::create(sizeof(TerrainDescriptors::Uniforms));
        _terrainDescriptors.data->properties.dataVariance = vsg::DYNAMIC_DATA;
        _terrainDescriptors.ubo = vsg::DescriptorBuffer::create(_terrainDescriptors.data, SETTINGS_UBO_BINDING);

        // initialize to the defaults
        auto& uniforms = *static_cast<TerrainDescriptors::Uniforms*>(_terrainDescriptors.data->dataPointer());
        uniforms = TerrainDescriptors::Uniforms();
    }

    // Just a StateGroup holding the graphics pipeline.
    // Descriptors are the global terrain uniform buffer and the VSG view-dependent buffer.
    stateGroup.add(pipelineConfig->bindGraphicsPipeline);
    stateGroup.add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));
    
    return true;
}

TerrainTileRenderModel
TerrainState::updateRenderModel(const TerrainTileRenderModel& oldRenderModel, const TerrainTileModel& dataModel, VSGContext& runtime) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(status.ok(), oldRenderModel);
    ROCKY_SOFT_ASSERT_AND_RETURN(pipelineConfig.valid(), oldRenderModel);

    // Copy the old one
    TerrainTileRenderModel renderModel = oldRenderModel;
    TerrainTileDescriptors& descriptors = renderModel.descriptors;

    if (dataModel.colorLayers.size() > 0 && dataModel.colorLayers[0].image.valid())
    {
        auto& layer = dataModel.colorLayers[0];

        renderModel.color.name = "color " + layer.key.str();
        renderModel.color.image = layer.image.image();
        renderModel.color.matrix = layer.matrix;

        // TODO: evaluate this 'clone' operation...
        auto data = util::wrapImageInVSG(renderModel.color.image);
        if (data)
        {
            // queue the old data for safe disposal
            if (descriptors.color)
                runtime->dispose(descriptors.color);

            // tell vsg to remove the image from CPU memory after sending it to the GPU
            // note, since we're using wrap() above, only the buffer will get deleted
            // and not the actual image data.
            data->properties.dataVariance = vsg::STATIC_DATA_UNREF_AFTER_TRANSFER;

            descriptors.color = vsg::DescriptorImage::create(
                texturedefs.color.sampler,
                data,
                texturedefs.color.uniform_binding,
                0, // array element (TODO: increment if we change to an array)
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            descriptors.color->setValue("name", renderModel.color.name);
        }
    }

    if (dataModel.elevation.heightfield.valid())
    {
        renderModel.elevation.name = "elevation " + dataModel.elevation.key.str();
        renderModel.elevation.image = dataModel.elevation.heightfield.image();
        renderModel.elevation.matrix = dataModel.elevation.matrix;

        auto data = util::wrapImageInVSG(renderModel.elevation.image);
        if (data)
        {
            // queue the old data for safe disposal
            if (descriptors.elevation)
                runtime->dispose(descriptors.elevation);

            // tell vsg to remove the image from CPU memory after sending it to the GPU
            // note, since we're using wrap() above, only the buffer will get deleted
            // and not the actual image data.
            data->properties.dataVariance = vsg::STATIC_DATA_UNREF_AFTER_TRANSFER;

            descriptors.elevation = vsg::DescriptorImage::create(
                texturedefs.elevation.sampler,
                data,
                texturedefs.elevation.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            descriptors.elevation->setValue("name", renderModel.elevation.name);
        }
    }

    // the per-tile uniform block:
    auto ubo = vsg::ubyteArray::create(sizeof(TerrainTileDescriptors::Uniforms));
    auto& uniforms = *static_cast<TerrainTileDescriptors::Uniforms*>(ubo->dataPointer());
    uniforms.elevation_matrix = renderModel.elevation.matrix;
    uniforms.color_matrix = renderModel.color.matrix;
    uniforms.model_matrix = renderModel.modelMatrix;
    descriptors.uniforms = vsg::DescriptorBuffer::create(ubo, TILE_UBO_BINDING);

    // make the descriptor set, and include the terrain settings UBO
    auto descriptorSet = vsg::DescriptorSet::create(
        pipelineConfig->layout->setLayouts[0],
        vsg::Descriptors{ descriptors.elevation, descriptors.color, descriptors.uniforms, _terrainDescriptors.ubo }
    );

    //if (sharedObjects) sharedObjects->share(descriptorSet);

    // binds the descriptor set to the pipeline
    descriptors.bind = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineConfig->layout,
        0, // first set
        descriptorSet
    );

    // Compile the objects. Everything should be under the bind command.
    runtime->compile(descriptors.bind);

    return renderModel;
}

void
TerrainState::updateSettings(const TerrainSettings& settings)
{    
    auto& uniforms = *static_cast<TerrainDescriptors::Uniforms*>(_terrainDescriptors.data->dataPointer());

    if ((bool)uniforms.wireOverlay != settings.wireOverlay.value())
    {
        uniforms.wireOverlay = settings.wireOverlay.value();
        _terrainDescriptors.data->dirty();
    }
}
