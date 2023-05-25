
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IconState.h"
#include "Runtime.h"
#include "Utils.h"
#include "../Icon.h" // for IconStyle

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/Draw.h>

using namespace ROCKY_NAMESPACE;

#define VERT_SHADER "shaders/rocky.icon.vert"
#define FRAG_SHADER "shaders/rocky.icon.frag"
#define BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define TEXTURE_SET 0 // must match layout(set=X) in the shader uniform
#define TEXTURE_BINDING 2 // must match the layout(binding=X) in the shader uniform
#define VIEWPORT_BUFFER_SET 1 // hard-coded in VSG ViewDependentState
#define VIEWPORT_BUFFER_BINDING 1 // hard-coded in VSG ViewDependentState (set=1)


vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> IconState::pipelineConfig;
vsg::StateGroup::StateCommands IconState::pipelineStateCommands;
//vsg::ref_ptr<vsg::Sampler> IconState::sampler;
rocky::Status IconState::status;

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(Runtime& runtime)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(VERT_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(FRAG_SHADER, runtime.searchPaths),
            runtime.readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });

        // data uniform buffer (object style)
        shaderSet->addUniformBinding(
            "icon", "", 
            BUFFER_SET, BUFFER_BINDING, 
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Icon texture image
        shaderSet->addUniformBinding(
            "icon_texture", "",
            TEXTURE_SET, TEXTURE_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // VSG viewport state
        shaderSet->addUniformBinding(
            "vsg_viewports", "",
            VIEWPORT_BUFFER_SET, VIEWPORT_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);


        return shaderSet;
    }
}

IconState::~IconState()
{
    pipelineConfig = nullptr;
    pipelineStateCommands.clear();
}

void
IconState::initialize(Runtime& runtime)
{
    // Now create the pipeline and stategroup to bind it
    if (!pipelineConfig)
    {
        auto shaderSet = createShaderSet(runtime);

        if (!shaderSet)
        {
            status = Status(Status::ResourceUnavailable,
                "Mesh shaders are missing or corrupt. "
                "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
            return;
        }

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        pipelineConfig = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        pipelineConfig->shaderHints = runtime.shaderCompileSettings;

        // activate the arrays we intend to use
        pipelineConfig->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);

        // Temporary decriptors that we will use to set up the PipelineConfig.
        pipelineConfig->enableUniform("icon");
        pipelineConfig->enableUniform("vsg_viewports");

        pipelineConfig->enableTexture("icon_texture"); // descriptors, "icon_texture", {}, sampler);

        // Alpha blending to support line smoothing
        pipelineConfig->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ {
            true,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        } };

        pipelineConfig->rasterizationState->cullMode = VK_CULL_MODE_NONE;

        // No depth testing please
        pipelineConfig->depthStencilState->depthCompareOp = VK_COMPARE_OP_ALWAYS;
        pipelineConfig->depthStencilState->depthTestEnable = VK_FALSE;
        pipelineConfig->depthStencilState->depthWriteEnable = VK_FALSE;

        // Register the ViewDescriptorSetLayout (for view-dependent state stuff
        // like viewpoint and lights data)
        // The "set" in GLSL's "layout(set=X, binding=Y)" refers to the index of
        // the descriptor set layout within the pipeline layout. Setting the
        // "additional" DSL appends it to the pipline layout, giving it set=1.
        pipelineConfig->additionalDescriptorSetLayout =
            runtime.sharedObjects ? runtime.sharedObjects->shared_default<vsg::ViewDescriptorSetLayout>() :
            vsg::ViewDescriptorSetLayout::create();

        // Initialize GraphicsPipeline from the data in the configuration.
        if (runtime.sharedObjects)
            runtime.sharedObjects->share(pipelineConfig, [](auto gpc) { gpc->init(); });
        else
            pipelineConfig->init();
    }

    vsg::StateGroup::StateCommands commands;

    commands.push_back(pipelineConfig->bindGraphicsPipeline);

    // assign any custom ArrayState that may be required
    //stateGroup->prototypeArrayState = shaderSet->getSuitableArrayState(defines);

    // This binds the view-dependent state from VSG (lights, viewport, etc.)
    auto bindViewDescriptorSets = vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout, 1);
    commands.push_back(bindViewDescriptorSets);

    if (runtime.sharedObjects)
        runtime.sharedObjects->share(bindViewDescriptorSets);

    pipelineStateCommands = commands;
}




BindIconStyle::BindIconStyle()
{
    ROCKY_HARD_ASSERT_STATUS(IconState::status);

    this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    this->firstSet = 0;
    this->layout = IconState::pipelineConfig->layout;

    //vsg::Descriptors descriptors;

    // tell VSG that the contents can change, and if they do, the data should be
    // transfered to the GPU before or during recording.
    styleData = vsg::ubyteArray::create(sizeof(IconStyle));
    styleData->properties.dataVariance = vsg::DYNAMIC_DATA;

    setStyle(IconStyle{});

    rebuildDescriptorSet();
}

void
BindIconStyle::setImage(std::shared_ptr<Image> in_image)
{
    my_image = in_image;

    // UNTESTED! might break, crash, and destroy
    rebuildDescriptorSet();
}


std::shared_ptr<Image>
BindIconStyle::image() const
{
    return my_image;
}

void
BindIconStyle::rebuildDescriptorSet()
{
    auto ubo = vsg::DescriptorBuffer::create(styleData, BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    auto tex_data = util::moveImageToVSG(my_image);

    // A sampler for the texture:
    auto sampler = vsg::Sampler::create();
    sampler->maxLod = 5; // this alone will prompt mipmap generation!
    sampler->minFilter = VK_FILTER_LINEAR;
    sampler->magFilter = VK_FILTER_LINEAR;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->anisotropyEnable = VK_TRUE; // don't need this for a billboarded icon
    sampler->maxAnisotropy = 4.0f;
    //if (runtime.sharedObjects)
    //    runtime.sharedObjects->share(sampler);

    auto tex = vsg::DescriptorImage::create(
        sampler, // IconState::sampler,
        tex_data,
        TEXTURE_BINDING,
        0, // array element (TODO: increment when we change to an array)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    this->layout = IconState::pipelineConfig->layout;

    this->descriptorSet = vsg::DescriptorSet::create(
        IconState::pipelineConfig->layout->setLayouts.front(),
        vsg::Descriptors{ ubo, tex });
}

void
BindIconStyle::setStyle(const IconStyle& value)
{
    IconStyle& my_style = *static_cast<IconStyle*>(styleData->dataPointer());
    my_style = value;
    styleData->dirty();
}

const IconStyle&
BindIconStyle::style() const
{
    return *static_cast<IconStyle*>(styleData->dataPointer());
}



IconGeometry::IconGeometry()
{
    _drawCommand = vsg::Draw::create(6, 1, 0, 0);
}

void
IconGeometry::compile(vsg::Context& context)
{
    commands.clear();

    std::vector<vsg::vec3> dummy_data(6);
    auto vert_array = vsg::vec3Array::create(6, dummy_data.data());
    assignArrays({ vert_array });
    commands.push_back(_drawCommand);

    vsg::Geometry::compile(context);
}
