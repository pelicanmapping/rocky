/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainState.h"
#include "Runtime.h"
#include "TerrainTileNode.h"
#include "Utils.h"
#include "PipelineState.h"

#include <rocky/Color.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>

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
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
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
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(texturedefs.color.sampler);

    texturedefs.elevation = { ELEVATION_TEX_NAME, ELEVATION_TEX_BINDING, vsg::Sampler::create(), {} };
    texturedefs.elevation.sampler->maxLod = 16;
    texturedefs.elevation.sampler->minFilter = VK_FILTER_LINEAR;
    texturedefs.elevation.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.elevation.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.elevation.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(texturedefs.elevation.sampler);

    texturedefs.normal = { NORMAL_TEX_NAME, NORMAL_TEX_BINDING, vsg::Sampler::create(), {} };
    texturedefs.normal.sampler->maxLod = 16;
    texturedefs.normal.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.normal.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    texturedefs.normal.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (_runtime.sharedObjects)
        _runtime.sharedObjects->share(texturedefs.normal.sampler);


    // Next make the "default" descriptor model, which is used when 
    // no other data is available. These are 1x1 pixel placeholder images.
    auto color_image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
    color_image->write(Color::Orange, 0, 0);
    texturedefs.color.defaultData = util::moveImageToVSG(color_image);
    ROCKY_HARD_ASSERT(texturedefs.color.defaultData);
    this->defaultTileDescriptors.color = vsg::DescriptorImage::create(
        texturedefs.color.sampler,
        texturedefs.color.defaultData,
        texturedefs.color.uniform_binding,
        0, // array element (TODO: increment when we change to an array)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto elev_image = Heightfield::create(1, 1);
    elev_image->fill(0.0f);
    texturedefs.elevation.defaultData = util::moveImageToVSG(elev_image);
    ROCKY_HARD_ASSERT(texturedefs.elevation.defaultData);
    this->defaultTileDescriptors.elevation = vsg::DescriptorImage::create(
        texturedefs.elevation.sampler,
        texturedefs.elevation.defaultData,
        texturedefs.elevation.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

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
    shaderSet->addDescriptorBinding(texturedefs.normal.name, "", 0, texturedefs.normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});
    shaderSet->addDescriptorBinding(TILE_BUFFER_NAME, "", 0, TILE_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, {});
    
    PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_FRAGMENT_BIT);

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

    // Temporary decriptors that we will use to set up the PipelineConfig.
    // Note, we only use these for setup, and then throw them away!
    // The ACTUAL descriptors we will make on a tile-by-tile basis.
#if 0
    config->assignTexture(textures.elevation.name, textures.elevation.defaultData, textures.elevation.sampler);
    config->assignTexture(textures.color.name, textures.color.defaultData, textures.color.sampler);
    config->assignTexture(textures.normal.name, textures.normal.defaultData, textures.normal.sampler);
#else
    config->enableTexture(texturedefs.elevation.name);
    config->enableTexture(texturedefs.color.name);
    config->enableTexture(texturedefs.normal.name);
#endif

    config->enableDescriptor(TILE_BUFFER_NAME);

    PipelineUtils::enableViewDependentData(config);

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
    stateGroup->add(PipelineUtils::createViewDependentBindCommand(pipelineConfig));

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
    ROCKY_SOFT_ASSERT_AND_RETURN(stategroup.valid(), void());

    // Takes a tile's render model (which holds the raw image and matrix data)
    // and creates the necessary VK data to render that model.

    // copy the existing one:
    TerrainTileDescriptors dm = renderModel.descriptors;

    if (renderModel.color.image)
    {
        auto data = util::moveImageToVSG(renderModel.color.image->clone());
        if (data)
        {
            // queue the old data for safe disposal
            runtime.dispose(dm.color);

            // tell vsg to remove the image from CPU memory after sending it to the GPU
            data->properties.dataVariance = vsg::STATIC_DATA_UNREF_AFTER_TRANSFER;

            dm.color = vsg::DescriptorImage::create(
                texturedefs.color.sampler,
                data,
                texturedefs.color.uniform_binding,
                0, // array element (TODO: increment if we change to an array)
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            dm.color->setValue("name", renderModel.color.name);
        }
    }

    if (renderModel.elevation.image)
    {
        auto data = util::moveImageToVSG(renderModel.elevation.image->clone());
        if (data)
        {
            // queue the old data for safe disposal
            runtime.dispose(dm.elevation);

            // tell vsg to remove the image from CPU memory after sending it to the GPU
            data->properties.dataVariance = vsg::STATIC_DATA_UNREF_AFTER_TRANSFER;

            dm.elevation = vsg::DescriptorImage::create(
                texturedefs.elevation.sampler,
                data,
                texturedefs.elevation.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            dm.elevation->setValue("name", renderModel.elevation.name);
        }
    }

    if (renderModel.normal.image)
    {
        auto data = util::moveImageToVSG(renderModel.normal.image->clone());
        if (data)
        {
            // queue the old data for safe disposal
            runtime.dispose(dm.normal);

            // tell vsg to remove the image from CPU memory after sending it to the GPU
            data->properties.dataVariance = vsg::STATIC_DATA_UNREF_AFTER_TRANSFER;

            dm.normal = vsg::DescriptorImage::create(
                texturedefs.normal.sampler,
                data,
                texturedefs.normal.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            dm.normal->setValue("name", renderModel.normal.name);
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
    dm.uniforms = vsg::DescriptorBuffer::create(data, TILE_BUFFER_BINDING);

    // the samplers:
    auto descriptorSetLayout = pipelineConfig->layout->setLayouts.front();

    auto descriptorSet = vsg::DescriptorSet::create(
        descriptorSetLayout,
        vsg::Descriptors{ dm.elevation, dm.color, dm.normal, dm.uniforms }
    );
    //if (sharedObjects) sharedObjects->share(descriptorSet);

    auto bind = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineConfig->layout,
        0, // first set
        descriptorSet
    );

    //ROCKY_HARD_ASSERT(bind->vdata().value._vkDescriptorSet == 0);

    // Destroy the old descriptor set(s) safely; don't just replace them
    // or it could cause a validataion error during compilation due to 
    // vsg descriptorset internal recycling.
    for (auto& command : stategroup->stateCommands)
        runtime.dispose(command);
    
    stategroup->stateCommands.clear();

    // Need to compile the descriptors
    runtime.compile(bind);

    // Temporary:
    // Delete the CPU memory assocaited with the rasters
    // now that they are compiled to the GPU.
    for (auto& dd : bind->descriptorSet->descriptors)
    {
        auto di = dd->cast<vsg::DescriptorImage>();
        if (di)
        {
            for (auto& ii : di->imageInfoList)
            {
                if (ii->imageView->image->data->properties.dataVariance == vsg::STATIC_DATA_UNREF_AFTER_TRANSFER)
                {
                    ii->imageView->image->data = nullptr;
                }
            }
        }
    }

    // And update the tile's state group
    stategroup->add(bind);
}
