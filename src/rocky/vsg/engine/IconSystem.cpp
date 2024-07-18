
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IconSystem.h"
#include "Runtime.h"
#include "Utils.h"
#include "PipelineState.h"
#include <rocky/Color.h>

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
        shaderSet->addDescriptorBinding(
            "icon", "", 
            BUFFER_SET, BUFFER_BINDING, 
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {});

        // Icon texture image
        shaderSet->addDescriptorBinding(
            "icon_texture", "",
            TEXTURE_SET, TEXTURE_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }
}

void
IconSystemNode::initialize(Runtime& runtime)
{
    auto shaderSet = createShaderSet(runtime);

    if (!shaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Icon shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    helper.pipelines.resize(NUM_PIPELINES);

    // create all pipeline permutations.
    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        auto& c = helper.pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        c.config->shaderHints = runtime.shaderCompileSettings;

        // activate the arrays we intend to use
        c.config->enableArray("in_vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);

        c.config->enableDescriptor("icon");
        c.config->enableTexture("icon_texture");

        PipelineUtils::enableViewDependentData(c.config);

        struct SetPipelineStates : public vsg::Visitor
        {
            int feature_mask;
            SetPipelineStates(int feature_mask_) : feature_mask(feature_mask_) { }
            void apply(vsg::Object& object) override {
                object.traverse(*this);
            }
            void apply(vsg::RasterizationState& state) override {
                state.cullMode = VK_CULL_MODE_NONE;
            }
            void apply(vsg::DepthStencilState& state) override {
                state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
                state.depthTestEnable = VK_FALSE;
                state.depthWriteEnable = VK_FALSE;
            }
            void apply(vsg::ColorBlendState& state) override {
                state.attachments = vsg::ColorBlendState::ColorBlendAttachments{
                    { true,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
                };
            }
        };
        c.config->accept(SetPipelineStates(feature_mask));

        c.config->init();

        c.commands = vsg::Commands::create();
        c.commands->addChild(c.config->bindGraphicsPipeline);
        c.commands->addChild(PipelineUtils::createViewDependentBindCommand(c.config));
    }
}

int IconSystemNode::featureMask(const Icon& component)
{
    return 0;
}



BindIconStyle::BindIconStyle()
{
    // nop
}

void
BindIconStyle::updateStyle(const IconStyle& value)
{
    if (!_styleData)
    {
        _styleData = vsg::ubyteArray::create(sizeof(IconStyle));

        // tells VSG that the contents can change, and if they do, the data should be
        // transfered to the GPU before or during recording.
        _styleData->properties.dataVariance = vsg::DYNAMIC_DATA;
    }

    IconStyle& my_style = *static_cast<IconStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
}

void
BindIconStyle::init(vsg::ref_ptr<vsg::PipelineLayout> layout)
{
    vsg::Descriptors descriptors;

    auto ubo = vsg::DescriptorBuffer::create(_styleData, BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptors.emplace_back(ubo);

    std::shared_ptr<Image> image = _image;
    if (!image)
    {
        image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
        image->write(Color::Red, 0, 0);
    }

    if (image)
    {
        auto tex_data = util::moveImageToVSG(image);

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

        auto tex = vsg::DescriptorImage::create(
            sampler, // IconState::sampler,
            tex_data,
            TEXTURE_BINDING,
            0, // array element (TODO: increment when we change to an array)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        descriptors.emplace_back(tex);
    }

    this->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    this->layout = layout;
    this->firstSet = 0;
    this->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(), descriptors);
}


IconGeometry::IconGeometry()
{
    _drawCommand = vsg::Draw::create(6, 1, 0, 0);
}

void
IconGeometry::compile(vsg::Context& context)
{
    if (commands.empty())
    {
        std::vector<vsg::vec3> dummy_data(6);
        auto vert_array = vsg::vec3Array::create(6, dummy_data.data());
        assignArrays({ vert_array });

        commands.push_back(_drawCommand);
    }

    vsg::Geometry::compile(context);
}
