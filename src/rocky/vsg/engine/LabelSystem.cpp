
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LabelSystem.h"
#include "Runtime.h"
#include "Utils.h"
#include "PipelineState.h"

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

using namespace ROCKY_NAMESPACE;

#define LABEL_MAX_NUM_CHARS 255

LabelSystemNode::LabelSystemNode(entt::registry& registry) :
    vsg::Inherit<ECS::SystemNode, LabelSystemNode>(registry),
    helper(registry)
{
    helper.initializeComponent = [this](Label& label, InitContext& context)
        { 
            this->initializeComponent(label, context);
        };

    text_technique_shared = vsg::GpuLayoutTechnique::create();
}

void
LabelSystemNode::initializeSystem(Runtime& runtime)
{
    // NOTE. This is temporary - replace with one or more TextGroup objects
    // to optimize rendering.

    // Configure the (global?) text shader set to turn off depth testing
    auto& options = runtime.readerWriterOptions;
    auto shaderSet = options->shaderSets["text"] = vsg::createTextShaderSet(options);

    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_FALSE;
    depthStencilState->depthWriteEnable = VK_FALSE;
    shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
}

void
LabelSystemNode::initializeComponent(Label& label, InitContext& context)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(label.style.font.valid(), void());

    label.options = context.readerWriterOptions;

    float size = label.style.pointSize;

    // Billboard = false because of https://github.com/vsg-dev/VulkanSceneGraph/discussions/985
    // Workaround: use a PixelScaleTransform with unrotate=true
    label.layout = vsg::StandardLayout::create();
    label.layout->billboard = false;
    label.layout->billboardAutoScaleDistance = 0.0f;
    label.layout->position = label.style.pixelOffset; // vsg::vec3(0.0f, 0.0f, 0.0f);
    label.layout->horizontal = vsg::vec3(size, 0.0f, 0.0f);
    label.layout->vertical = vsg::vec3(0.0f, size, 0.0f); // layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
    label.layout->color = vsg::vec4(1.0f, 0.9f, 1.0f, 1.0f);
    label.layout->outlineWidth = label.style.outlineSize;
    label.layout->horizontalAlignment = label.style.horizontalAlignment;
    label.layout->verticalAlignment = label.style.verticalAlignment;
    context.sharedObjects->share(label.layout);

    label.valueBuffer = vsg::stringValue::create(label.text);

    label.textNode = vsg::Text::create();
    label.textNode->font = label.style.font;
    label.textNode->text = label.valueBuffer;
    label.textNode->layout = label.layout;
    label.textNode->technique = text_technique_shared;
    label.textNode->setup(LABEL_MAX_NUM_CHARS, label.options); // allocate enough space for max possible characters?

#if 0
    label.node = label.textNode;
#else
    auto pst = PixelScaleTransform::create();
    pst->unrotate = true;
    pst->addChild(label.textNode);
    label.node = pst;
#endif
}
