
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

bool
LabelSystemNode::update(entt::entity entity, Runtime& runtime)
{
    auto& label = registry.get<Label>(entity);
    auto& renderable = registry.get<ECS::Renderable>(label.entity);

    // TODO: allow custom fonts
    ROCKY_SOFT_ASSERT_AND_RETURN(label.style.font.valid(), true);

    if (renderable.node)
        runtime.dispose(renderable.node);

    auto options = runtime.readerWriterOptions;

    float size = label.style.pointSize;

    // Billboard = false because of https://github.com/vsg-dev/VulkanSceneGraph/discussions/985
    // Workaround: use a PixelScaleTransform with unrotate=true
    auto layout = vsg::StandardLayout::create();
    layout->billboard = false;
    layout->billboardAutoScaleDistance = 0.0f;
    layout->position = label.style.pixelOffset; // vsg::vec3(0.0f, 0.0f, 0.0f);
    layout->horizontal = vsg::vec3(size, 0.0f, 0.0f);
    layout->vertical = vsg::vec3(0.0f, size, 0.0f); // layout->billboard ? vsg::vec3(0.0, size, 0.0) : vsg::vec3(0.0, 0.0, size);
    layout->color = vsg::vec4(1.0f, 0.9f, 1.0f, 1.0f);
    layout->outlineWidth = label.style.outlineSize;
    layout->horizontalAlignment = label.style.horizontalAlignment;
    layout->verticalAlignment = label.style.verticalAlignment;

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
    rd.node = label.textNode;
#else
    auto pst = PixelScaleTransform::create();
    pst->unrotate = true;
    pst->addChild(textNode);
    renderable.node = pst;
#endif

    runtime.compile(renderable.node);
    return true;
}
