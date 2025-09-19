
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IconSystem.h"
#include "../VSGContext.h"
#include "../PipelineState.h"
#include "../VSGUtils.h"
#include <rocky/Color.h>

using namespace ROCKY_NAMESPACE;

#define VERT_SHADER "shaders/rocky.icon.vert"
#define FRAG_SHADER "shaders/rocky.icon.frag"

#define BUFFER_SET 0 // must match layout(set=X) in the shader UBO
#define BUFFER_BINDING 1 // must match the layout(binding=X) in the shader UBO (set=0)
#define TEXTURE_SET 0 // must match layout(set=X) in the shader uniform
#define TEXTURE_BINDING 2 // must match the layout(binding=X) in the shader uniform

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createShaderSet(VSGContext& context)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(VERT_SHADER, context->searchPaths),
            context->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(FRAG_SHADER, context->searchPaths),
            context->readerWriterOptions);

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

IconSystemNode::IconSystemNode(Registry& registry) :
    Inherit(registry)
{
    //nop
}
void
IconSystemNode::initialize(VSGContext& context)
{
    auto shaderSet = createShaderSet(context);

    if (!shaderSet)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Icon shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    pipelines.resize(NUM_PIPELINES);

    // create all pipeline permutations.
    for (int feature_mask = 0; feature_mask < NUM_PIPELINES; ++feature_mask)
    {
        //auto& c = helper.pipelines[feature_mask];
        auto& c = pipelines[feature_mask];

        // Create the pipeline configurator for terrain; this is a helper object
        // that acts as a "template" for terrain tile rendering state.
        c.config = vsg::GraphicsPipelineConfig::create(shaderSet);

        // Apply any custom compile settings / defines:
        c.config->shaderHints = context->shaderCompileSettings;

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
        SetPipelineStates visitor(feature_mask);
        c.config->accept(visitor);

        c.config->init();

        c.commands = vsg::Commands::create();
        c.commands->addChild(c.config->bindGraphicsPipeline);
        c.commands->addChild(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, c.config->layout, VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));

    }
}

namespace
{
    template<typename CACHE, typename MUTEX, typename CREATE>
    vsg::ref_ptr<vsg::DescriptorImage>& getOrCreate(CACHE& cache, MUTEX& mutex, std::shared_ptr<Image> key, CREATE&& create)
    {
        std::scoped_lock L(mutex);
        auto iter = cache.find(key);
        if (iter != cache.end())
            return iter->second;
        else
            return cache[key] = create();
    }
}

void
IconSystemNode::createOrUpdateNode(const Icon& icon, detail::BuildInfo& data, VSGContext& vsgcontext) const
{
    bool rebuild = data.existing_node == nullptr;

    if (!rebuild)
    {
        auto bindCommand = util::find<BindIconStyle>(data.existing_node);
        std::shared_ptr<Image> old_image;
        if (bindCommand->getValue("icon_image", old_image) && icon.image != old_image)
        {
            rebuild = true;
        }
    }

    vsg::ref_ptr<BindIconStyle> bindCommand;

    if (rebuild)
    {
        auto geometry = IconGeometry::create();

        bindCommand = BindIconStyle::create();
        bindCommand->setValue("icon_image", icon.image);
        bindCommand->updateStyle(icon.style);

        // assemble the descriptor set for this icon:
        vsg::Descriptors descriptors;

        // uniform buffer object for dynamic data:
        bindCommand->_ubo = vsg::DescriptorBuffer::create(bindCommand->_styleData, BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptors.emplace_back(bindCommand->_ubo);

        // make a default image if we don't have one
        auto image = icon.image;
        if (!image)
        {
            image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
            image->write(Color::Red, 0, 0);
        }

        // check the cache.
        auto& descriptorImage = getOrCreate(descriptorImage_cache, mutex, icon.image, [&]()
            {
                auto imageData = util::moveImageToVSG(image);

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

                return vsg::DescriptorImage::create(
                    sampler,
                    imageData,
                    TEXTURE_BINDING,
                    0, // array element (TODO: increment when we change to an array)
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            });

        descriptors.emplace_back(descriptorImage);

        auto layout = getPipelineLayout(icon);
        bindCommand->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        bindCommand->layout = layout;
        bindCommand->firstSet = 0;
        bindCommand->descriptorSet = vsg::DescriptorSet::create(layout->setLayouts.front(), descriptors);

        auto stateGroup = vsg::StateGroup::create();
        stateGroup->stateCommands.push_back(bindCommand);
        stateGroup->addChild(geometry);

        data.new_node = stateGroup;
    }

    else
    {
        bindCommand = util::find<BindIconStyle>(data.existing_node);
        bindCommand->updateStyle(icon.style);    
    }

    vsg::ModifiedCount mc;
    if (bindCommand->_styleData->getModifiedCount(mc) && mc.count > 0)
    {
        vsgcontext->upload(bindCommand->_ubo->bufferInfoList);
    }
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
        // do NOT mark as DYNAMIC_DATA, since we only update it when the style changes.
    }

    IconStyle& my_style = *static_cast<IconStyle*>(_styleData->dataPointer());
    my_style = value;
    _styleData->dirty();
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
