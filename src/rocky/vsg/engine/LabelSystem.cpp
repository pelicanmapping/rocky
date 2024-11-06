
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
    Inherit(registry)
{
    //nop
}

void
LabelSystemNode::initializeSystem(Runtime& runtime)
{
    // Configure the (global?) text shader set to turn off depth testing
    auto& options = runtime.readerWriterOptions;
    auto shaderSet = options->shaderSets["text"] = vsg::createTextShaderSet(options);

    auto depthStencilState = vsg::DepthStencilState::create();
    depthStencilState->depthTestEnable = VK_FALSE;
    depthStencilState->depthWriteEnable = VK_FALSE;
    shaderSet->defaultGraphicsPipelineStates.push_back(depthStencilState);
}

vsg::ref_ptr<vsg::Node>
LabelSystemNode::createNode(entt::entity entity, Runtime& runtime) const
{
    auto& label = registry.get<Label>(entity);

    // TODO: allow custom fonts
    ROCKY_SOFT_ASSERT_AND_RETURN(label.style.font.valid(), {});

    auto options = runtime.readerWriterOptions;

    float size = label.style.pointSize;

    // Billboard = false because of https://github.com/vsg-dev/VulkanSceneGraph/discussions/985
    // Workaround: use a PixelScaleTransform with unrotate=true
#if 1
    auto layout = vsg::StandardLayout::create();
    layout->billboard = false;
    layout->billboardAutoScaleDistance = 0.0f;
    layout->position = label.style.pixelOffset;
    layout->horizontal = vsg::vec3(size, 0.0f, 0.0f);
    layout->vertical = vsg::vec3(0.0f, size, 0.0f); // layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
    layout->color = vsg::vec4(1.0f, 0.9f, 1.0f, 1.0f);
    layout->outlineWidth = label.style.outlineSize;
    layout->horizontalAlignment = label.style.horizontalAlignment;
    layout->verticalAlignment = label.style.verticalAlignment;
#else
    auto layout = vsg::StandardLayout::create();
    layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
    layout->position = vsg::vec3(0.0, 0.0, 0.0);
    layout->horizontal = vsg::vec3(size, 0.0, 0.0);
    layout->vertical = vsg::vec3(0.0, size, 0.0);
    layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);
    layout->outlineWidth = 0.1f;
    layout->billboard = true;
#endif

    // Share this since it should be the same for everything
    if (runtime.sharedObjects)
        runtime.sharedObjects->share(layout);

    auto valueBuffer = vsg::stringValue::create(label.text);

    auto textNode = vsg::Text::create();
    textNode->font = label.style.font;
    textNode->text = valueBuffer;
    textNode->layout = layout;
    textNode->technique = vsg::GpuLayoutTechnique::create(); // cannot share these!
    textNode->setup(LABEL_MAX_NUM_CHARS, options); // allocate enough space for max possible characters?

#if 0
    renderable.node = textNode;
#else
    auto pst = PixelScaleTransform::create();
    pst->unrotate = true;
    pst->addChild(textNode);
    return pst;
    //renderable.node = pst;
#endif
}
