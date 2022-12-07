/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "StateFactory.h"
#include "TerrainTileNode.h"
#include "TileRenderModel.h"

#include <rocky/Color.h>
#include <rocky/Heightfield.h>
#include <rocky/Image.h>
#include <rocky/Notify.h>
#include <rocky_vsg/Utils.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>

#define ELEVATION_TEX_NAME "elevation_tex"
#define ELEVATION_TEX_BINDING 10

#define COLOR_TEX_NAME "color_tex"
#define COLOR_TEX_BINDING 11

#define NORMAL_TEX_NAME "normal_tex"
#define NORMAL_TEX_BINDING 12

using namespace rocky;

StateFactory::StateFactory()
{
    // set up the texture samplers and default images we will use to render terrain.
    createDefaultDescriptors();

    // shader set prototype for use with a GraphicsPipelineConfig.
    shaderSet = createShaderSet();

    // cache of shared objects in the terrain state. Re-using objects
    // helps performance.
    sharedObjects = vsg::SharedObjects::create();

    // create the pipeline configurator for terrain; this is a helper object
    // that acts as a "template" for terrain tile rendering state.
    pipelineConfig = createPipelineConfig(sharedObjects.get());
}

void
StateFactory::createDefaultDescriptors()
{
    // First create our samplers - each one is shared across all tiles.

    // color channel
    // TODO: more than one - make this an array?
    // TODO: activate mipmapping
    _color = { COLOR_TEX_NAME, COLOR_TEX_BINDING, vsg::Sampler::create(), {} };
    _color.sampler->maxLod = 16; // 0; // mipmapLevelsHint;
    //color_sampler->minFilter = mipmap filter!
    _color.sampler->minFilter = VK_FILTER_LINEAR;
    _color.sampler->magFilter = VK_FILTER_NEAREST;
    _color.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _color.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _color.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _color.sampler->anisotropyEnable = VK_TRUE;
    _color.sampler->maxAnisotropy = 4.0f;
    if (sharedObjects) sharedObjects->share(_color.sampler);

    _elevation = { ELEVATION_TEX_NAME, ELEVATION_TEX_BINDING, vsg::Sampler::create(), {} };
    _elevation.sampler->maxLod = 16;
    _elevation.sampler->minFilter = VK_FILTER_LINEAR;
    _elevation.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _elevation.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _elevation.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (sharedObjects) sharedObjects->share(_elevation.sampler);

    _normal = { NORMAL_TEX_NAME, NORMAL_TEX_BINDING, vsg::Sampler::create(), {} };
    _normal.sampler->maxLod = 16;
    _normal.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _normal.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    _normal.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (sharedObjects) sharedObjects->share(_normal.sampler);

    // Next make the "default" descriptor model, which is used when 
    // no other data is available.

    auto color_image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
    color_image->write(Color::Yellow, 0, 0);
    //color_image->fill(Color::Yellow);
    _color.defaultData = convertImageToVSG(color_image);
    ROCKY_HARD_ASSERT(_color.defaultData);
    this->defaultTileDescriptors.color = vsg::DescriptorImage::create(
        _color.sampler, _color.defaultData, _color.uniform_binding,
        0, // array element (TODO: increment when we change to an array)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto elev_image = Heightfield::create(1, 1);
    elev_image->fill(0.0f);
    _elevation.defaultData = convertImageToVSG(elev_image);
    ROCKY_HARD_ASSERT(_elevation.defaultData);
    this->defaultTileDescriptors.elevation = vsg::DescriptorImage::create(
        _elevation.sampler, _elevation.defaultData, _elevation.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto normal_image = Image::create(Image::R8G8B8_UNORM, 1, 1);
    normal_image->fill(fvec4(.5, .5, 1, 0));
    _normal.defaultData = convertImageToVSG(normal_image);
    ROCKY_HARD_ASSERT(_normal.defaultData);
    this->defaultTileDescriptors.normal = vsg::DescriptorImage::create(
        _normal.sampler, _normal.defaultData, _normal.uniform_binding,
        0, // array element
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

vsg::ref_ptr<vsg::ShaderSet>
StateFactory::createShaderSet() const
{
    vsg::ref_ptr<vsg::ShaderSet> shaderSet;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    searchPaths.push_back(vsg::Path("H:/devel/rocky/install/share"));

    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vsg::findFile("terrain.vert", searchPaths));

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        vsg::findFile("terrain.frag", searchPaths));

    ROCKY_SOFT_ASSERT_AND_RETURN(
        vertexShader && fragmentShader,
        shaderSet,
        "Could not create terrain shaders");

    // TODO
#if 1
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
    shaderSet->addAttributeBinding("inVertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNormal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inUV", "", 2, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNeighborVertex", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    shaderSet->addAttributeBinding("inNeighborNormal", "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec3Array::create(1));
    //shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    // "binding" (4th param) must match "layout(location=X) uniform" in the shader
    shaderSet->addUniformBinding(_elevation.name, "", 0, _elevation.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, {}); // , vsg::floatArray2D::create(1, 1));
    shaderSet->addUniformBinding(_color.name, "", 0, _color.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {}); // , vsg::vec3Array2D::create(1, 1));
    shaderSet->addUniformBinding(_normal.name, "", 0, _normal.uniform_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {}); // , vsg::vec3Array2D::create(1, 1));
    //shaderSet->addUniformBinding(_colorParent.name, "", 0, _colorParent.location, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));

    //shaderSet->addUniformBinding("displacementMap", "VSG_DISPLACEMENT_MAP", 0, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::vec4Array2D::create(1, 1));
    //shaderSet->addUniformBinding("diffuseMap", "VSG_DIFFUSE_MAP", 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1));
    //shaderSet->addUniformBinding("material", "", 0, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PhongMaterialValue::create());

    // Consider the sizing here (128?)
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    //shaderSet->optionalDefines = { "VSG_POINT_SPRITE", "VSG_GREYSACLE_DIFFUSE_MAP" };

    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create() });
    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create() });
    //shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{ {"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create() });

    return shaderSet;
}


vsg::ref_ptr<vsg::GraphicsPipelineConfig>
StateFactory::createPipelineConfig(vsg::SharedObjects* sharedObjects) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(shaderSet.valid(), {});

    // This method uses the "terrainShaderSet" as a prototype to
    // define a graphics pipeline that will render the terrain.

    auto pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

    // activate the arrays we intend to use
    pipelineConfig->enableArray("inVertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray("inNormal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray("inUV", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray("inNeighborVertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    pipelineConfig->enableArray("inNeighborNormal", VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // wireframe rendering:
    //pipelineConfig->rasterizationState->polygonMode = VK_POLYGON_MODE_LINE;

    // Temporary decriptors that we will use to set up the PipelineConfig.
    // Note, we only use these for setup, and then throw them away!
    // The ACTUAL descriptors we will make on a tile-by-tile basis.
    vsg::Descriptors descriptors;
    pipelineConfig->assignTexture(descriptors, _elevation.name, _elevation.defaultData, _elevation.sampler);
    pipelineConfig->assignTexture(descriptors, _color.name, _color.defaultData, _color.sampler);
    pipelineConfig->assignTexture(descriptors, _normal.name, _normal.defaultData, _normal.sampler);

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

    // register the ViewDescriptorSetLayout.
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


vsg::ref_ptr<vsg::StateGroup>
StateFactory::createTerrainStateGroup() const
{
    // create StateGroup as the root of the scene/command graph to hold
    // the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    stateGroup->add(pipelineConfig->bindGraphicsPipeline);

    return stateGroup;
}

TileDescriptorModel
StateFactory::createTileDescriptorModel(const TileRenderModel& renderModel) const
{
    auto stategroup = vsg::StateGroup::create();

    // copy the existing one:
    TileDescriptorModel dm = renderModel.descriptorModel;

    if (renderModel.color.image)
    {
        auto data = convertImageToVSG(renderModel.color.image);
        if (data)
        {
            dm.color = vsg::DescriptorImage::create(
                _color.sampler, data, _color.uniform_binding,
                0, // array element (TODO: increment when we change to an array)
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    if (renderModel.elevation.image)
    {
        auto data = convertImageToVSG(renderModel.elevation.image);
        if (data)
        {
            dm.elevation = vsg::DescriptorImage::create(
                _elevation.sampler, data, _elevation.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    if (renderModel.normal.image)
    {
        auto data = convertImageToVSG(renderModel.normal.image);
        if (data)
        {
            dm.normal = vsg::DescriptorImage::create(
                _normal.sampler, data, _normal.uniform_binding,
                0, // array element
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
    }

    auto descriptorSet = vsg::DescriptorSet::create(
        pipelineConfig->descriptorSetLayout,
        vsg::Descriptors{ dm.elevation, dm.color, dm.normal }
    );
    if (sharedObjects) sharedObjects->share(descriptorSet);

    dm.bindDescriptorSetCommand = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineConfig->layout.get(),
        0, // first set
        descriptorSet
    );
    if (sharedObjects) sharedObjects->share(dm.bindDescriptorSetCommand);

    return dm;
}

void
StateFactory::refreshStateGroup(TerrainTileNode* tile) const
{
    auto& renderModel = tile->renderModel();

    // make a new descriptor model for this tile
    renderModel.descriptorModel = createTileDescriptorModel(renderModel);

    tile->stateGroup()->stateCommands.clear();
    tile->stateGroup()->add(renderModel.descriptorModel.bindDescriptorSetCommand);

#if 0
    // assign any custom ArrayState that may be required.
    stateGroup()->prototypeArrayState = terrain.stateFactory.shaderSet->getSuitableArrayState(
        terrain.stateFactory.pipelineConfig->shaderHints->defines);

    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        terrain.stateFactory.pipelineConfig->layout,
        1);

    stateGroup()->add(bindViewDescriptorSets);
#endif
}
