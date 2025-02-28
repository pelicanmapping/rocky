
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LabelSystem.h"
#include "../PipelineState.h"
#include "../Utils.h"

#include <rocky/vsg/PixelScaleTransform.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/Draw.h>

#include <vsg/vk/State.h>
#include <vsg/text/StandardLayout.h>
#include <vsg/text/CpuLayoutTechnique.h>
#include <vsg/text/GpuLayoutTechnique.h>
#include <vsg/text/Font.h>
#include <vsg/state/DepthStencilState.h>
#include <vsg/io/read.h>

#include <algorithm>

using namespace ROCKY_NAMESPACE;


#define LABEL_MAX_NUM_CHARS 255


namespace
{
    /**
    * Custom layout technique that only issues the geometry drawing commands
    * and skips the decriptor and pipeline binds. When grouping lots of text draws together
    * we will do the pipeline/descriptor binding once per font elsewhere. This is a nice
    * perfromance boost.
    * 
    * This is mostly copied from vsg::CpuLayoutTechnique
    */
    class RockysCpuLayoutTechnique : public vsg::Inherit<vsg::CpuLayoutTechnique, RockysCpuLayoutTechnique>
    {
    public:
        vsg::ref_ptr<vsg::Node> createRenderingSubgraph(vsg::ref_ptr<vsg::ShaderSet> shaderSet, vsg::ref_ptr<vsg::Font> font, bool billboard, vsg::TextQuads& quads, uint32_t minimumAllocation) override
        {
            vsg::vec4 color = quads.front().colors[0];
            vsg::vec4 outlineColor = quads.front().outlineColors[0];
            float outlineWidth = quads.front().outlineWidths[0];
            vsg::vec4 centerAndAutoScaleDistance = quads.front().centerAndAutoScaleDistance;
            bool singleColor = true;
            bool singleOutlineColor = true;
            bool singleOutlineWidth = true;
            bool singleCenterAutoScaleDistance = true;
            for (auto& quad : quads)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (quad.colors[i] != color) singleColor = false;
                    if (quad.outlineColors[i] != outlineColor) singleOutlineColor = false;
                    if (quad.outlineWidths[i] != outlineWidth) singleOutlineWidth = false;
                }
                if (quad.centerAndAutoScaleDistance != centerAndAutoScaleDistance) singleCenterAutoScaleDistance = false;
            }

            uint32_t num_quads = std::max(static_cast<uint32_t>(quads.size()), minimumAllocation);
            uint32_t num_vertices = num_quads * 4;
            uint32_t num_colors = singleColor ? 1 : num_vertices;
            uint32_t num_outlineColors = singleOutlineColor ? 1 : num_vertices;
            uint32_t num_outlineWidths = singleOutlineWidth ? 1 : num_vertices;
            //uint32_t num_centerAndAutoScaleDistances = billboard ? (singleCenterAutoScaleDistance ? 1 : num_vertices) : 0;

            if (!vertices || num_vertices > vertices->size()) vertices = vsg::vec3Array::create(num_vertices);
            if (!colors || num_colors > colors->size()) colors = vsg::vec4Array::create(num_colors);
            if (!outlineColors || num_outlineColors > outlineColors->size()) outlineColors = vsg::vec4Array::create(num_outlineColors);
            if (!outlineWidths || num_outlineWidths > outlineWidths->size()) outlineWidths = vsg::floatArray::create(num_outlineWidths);
            if (!texcoords || num_vertices > texcoords->size()) texcoords = vsg::vec3Array::create(num_vertices);
            //if (billboard && (!centerAndAutoScaleDistances || num_centerAndAutoScaleDistances > centerAndAutoScaleDistances->size())) centerAndAutoScaleDistances = vsg::vec4Array::create(num_centerAndAutoScaleDistances);

            uint32_t vi = 0;

            float leadingEdgeGradient = 0.1f;

            if (singleColor) colors->set(0, color);
            if (singleOutlineColor) outlineColors->set(0, outlineColor);
            if (singleOutlineWidth) outlineWidths->set(0, outlineWidth);
            //if (singleCenterAutoScaleDistance && centerAndAutoScaleDistances) centerAndAutoScaleDistances->set(0, centerAndAutoScaleDistance);

            for (auto& quad : quads)
            {
                float leadingEdgeTilt = vsg::length(quad.vertices[0] - quad.vertices[1]) * leadingEdgeGradient;
                float topEdgeTilt = leadingEdgeTilt;

                vertices->set(vi, quad.vertices[0]);
                vertices->set(vi + 1, quad.vertices[1]);
                vertices->set(vi + 2, quad.vertices[2]);
                vertices->set(vi + 3, quad.vertices[3]);

                if (!singleColor)
                {
                    colors->set(vi, quad.colors[0]);
                    colors->set(vi + 1, quad.colors[1]);
                    colors->set(vi + 2, quad.colors[2]);
                    colors->set(vi + 3, quad.colors[3]);
                }

                if (!singleOutlineColor)
                {
                    outlineColors->set(vi, quad.outlineColors[0]);
                    outlineColors->set(vi + 1, quad.outlineColors[1]);
                    outlineColors->set(vi + 2, quad.outlineColors[2]);
                    outlineColors->set(vi + 3, quad.outlineColors[3]);
                }

                if (!singleOutlineWidth)
                {
                    outlineWidths->set(vi, quad.outlineWidths[0]);
                    outlineWidths->set(vi + 1, quad.outlineWidths[1]);
                    outlineWidths->set(vi + 2, quad.outlineWidths[2]);
                    outlineWidths->set(vi + 3, quad.outlineWidths[3]);
                }

                texcoords->set(vi + 0, vsg::vec3(quad.texcoords[0].x, quad.texcoords[0].y, leadingEdgeTilt + topEdgeTilt));
                texcoords->set(vi + 1, vsg::vec3(quad.texcoords[1].x, quad.texcoords[1].y, topEdgeTilt));
                texcoords->set(vi + 2, vsg::vec3(quad.texcoords[2].x, quad.texcoords[2].y, 0.0f));
                texcoords->set(vi + 3, vsg::vec3(quad.texcoords[3].x, quad.texcoords[3].y, leadingEdgeTilt));

                if (!singleCenterAutoScaleDistance && centerAndAutoScaleDistances)
                {
                    centerAndAutoScaleDistances->set(vi, quad.centerAndAutoScaleDistance);
                    centerAndAutoScaleDistances->set(vi + 1, quad.centerAndAutoScaleDistance);
                    centerAndAutoScaleDistances->set(vi + 2, quad.centerAndAutoScaleDistance);
                    centerAndAutoScaleDistances->set(vi + 3, quad.centerAndAutoScaleDistance);
                }

                vi += 4;
            }

            uint32_t num_indices = num_quads * 6;
            if (!indices || num_indices > indices->valueCount())
            {
                if (num_vertices > 65536) // check if requires uint or ushort indices
                {
                    auto ui_indices = vsg::uintArray::create(num_indices);
                    indices = ui_indices;

                    auto itr = ui_indices->begin();
                    vi = 0;
                    for (uint32_t i = 0; i < num_quads; ++i)
                    {
                        (*itr++) = vi;
                        (*itr++) = vi + 1;
                        (*itr++) = vi + 2;
                        (*itr++) = vi + 2;
                        (*itr++) = vi + 3;
                        (*itr++) = vi;

                        vi += 4;
                    }
                }
                else
                {
                    auto us_indices = vsg::ushortArray::create(num_indices);
                    indices = us_indices;

                    auto itr = us_indices->begin();
                    vi = 0;
                    for (uint32_t i = 0; i < num_quads; ++i)
                    {
                        (*itr++) = vi;
                        (*itr++) = vi + 1;
                        (*itr++) = vi + 2;
                        (*itr++) = vi + 2;
                        (*itr++) = vi + 3;
                        (*itr++) = vi;

                        vi += 4;
                    }
                }
            }

            if (!drawIndexed)
                drawIndexed = vsg::DrawIndexed::create(static_cast<uint32_t>(quads.size() * 6), 1, 0, 0, 0);
            else
                drawIndexed->indexCount = static_cast<uint32_t>(quads.size() * 6);

            vsg::DataList arrays = { vertices, colors, outlineColors, outlineWidths, texcoords };
            bindVertexBuffers = vsg::BindVertexBuffers::create(0, arrays);
            bindIndexBuffer = vsg::BindIndexBuffer::create(indices);

            // setup geometry
            auto drawCommands = vsg::Commands::create();
            drawCommands->addChild(bindVertexBuffers);
            drawCommands->addChild(bindIndexBuffer);
            drawCommands->addChild(drawIndexed);
            
            return drawCommands;
        }
    };
}


LabelSystemNode::LabelSystemNode(ecs::Registry& registry) :
    Inherit(registry)
{
    //nop
}

void
LabelSystemNode::initialize(VSGContext& runtime)
{
    // use the VSG stock shaders for text:
    auto& options = runtime->readerWriterOptions;
    auto shaderSet = options->shaderSets["text"] = vsg::createTextShaderSet(options);

    // Configure the text shader set to turn off depth testing
    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_FALSE;
    depthStencilState->depthWriteEnable = VK_FALSE;
    shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);

    auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    // Taken from vsg::CpuLayoutTechnique
    config->enableArray("inPosition", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    config->enableArray("inColor", VK_VERTEX_INPUT_RATE_INSTANCE, 16);
    config->enableArray("inOutlineColor", VK_VERTEX_INPUT_RATE_INSTANCE, 16);
    config->enableArray("inOutlineWidth", VK_VERTEX_INPUT_RATE_INSTANCE, 4);
    config->enableArray("inTexCoord", VK_VERTEX_INPUT_RATE_VERTEX, 12);

    // set up sampler for the font's texture atlas.
    auto sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_LINEAR;
    sampler->minFilter = VK_FILTER_LINEAR;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler->borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    sampler->anisotropyEnable = VK_FALSE; // don't need it since we're "billboarding"
    sampler->maxLod = 12.0; // generate mipmaps up to level 12

    if (runtime->sharedObjects)
        runtime->sharedObjects->share(sampler);

    // this will prompt the creation of a descriptor image and associated bind command
    // for the texture atlas
    config->assignTexture("textureAtlas", runtime->defaultFont->atlas, sampler);

    // cook it
    config->init();

    // copies any state commands from the configurator to the state group;
    // this will include the texture bind (the sampler) and the pipeline bind itself.
    auto stategroup = vsg::StateGroup::create();
    config->copyTo(stategroup, runtime->sharedObjects);

    // Initialize GraphicsPipeline from the data in the configuration.
    // Copy the state commands into our pipeline container.
    // TODO: one pipeline per font...?
    pipelines.resize(1);
    pipelines[0].config = config;
    pipelines[0].commands = vsg::Commands::create();
    for(auto& child : stategroup->stateCommands)
        pipelines[0].commands->children.push_back(child);
}

void
LabelSystemNode::createOrUpdateNode(Label& label, ecs::BuildInfo& data, VSGContext& runtime) const
{
    bool rebuild = data.existing_node == nullptr;

    if (data.existing_node)
    {
        auto textNode = util::find<vsg::Text>(data.existing_node);
        auto text = static_cast<vsg::stringValue*>(textNode->text.get());
        auto layout = static_cast<vsg::StandardLayout*>(textNode->layout.get());

        rebuild =
            text->value() != label.text ||
            layout->outlineWidth != label.style.outlineSize ||
            layout->horizontalAlignment != label.style.horizontalAlignment ||
            layout->verticalAlignment != label.style.verticalAlignment;
    }

    if (rebuild)
    {
        auto options = runtime->readerWriterOptions;
        //float size = label.style.pointSize;

        // We are doing our own billboarding with the PixelScaleTransform
        const float nativeSize = 128.0f;
        auto layout = vsg::StandardLayout::create();
        layout->billboard = false; // disabled intentionally
        layout->position = label.style.pixelOffset;
        layout->horizontal = vsg::vec3(nativeSize, 0.0f, 0.0f);
        layout->vertical = vsg::vec3(0.0f, nativeSize, 0.0f);
        layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        layout->outlineWidth = label.style.outlineSize;
        layout->horizontalAlignment = label.style.horizontalAlignment;
        layout->verticalAlignment = label.style.verticalAlignment;

        // Share this since it should be the same for everything
        if (runtime->sharedObjects)
            runtime->sharedObjects->share(layout);

        auto valueBuffer = vsg::stringValue::create(label.text);

        auto textNode = vsg::Text::create();
        textNode->font = label.style.font; // this has to be set or nothing shows up, don't know why yet
        textNode->text = valueBuffer;
        textNode->layout = layout;
        textNode->technique = RockysCpuLayoutTechnique::create(); // one per label yes
        textNode->setup(LABEL_MAX_NUM_CHARS, options); // allocate enough space for max possible characters?

        // don't need this since we're using the custom technique
        textNode->shaderSet = {};

        // this dude will billboard the label.
        auto pst = PixelScaleTransform::create();
        pst->unitSize = nativeSize;
        pst->renderSize = label.style.pointSize;
        pst->unrotate = true;
        pst->addChild(textNode);

        data.new_node = pst;
    }

    else
    {
        auto pst = util::find<PixelScaleTransform>(data.existing_node);
        pst->renderSize = label.style.pointSize;
    }
}
