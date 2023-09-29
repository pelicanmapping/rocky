
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "LabelSystem.h"
#include "Runtime.h"
#include "Utils.h"
#include "PipelineState.h"

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/Draw.h>

using namespace ROCKY_NAMESPACE;

LabelSystem::LabelSystem(entt::registry& registry) :
    vsg::Inherit<ECS::SystemNode, LabelSystem>(registry),
    helper(registry)
{
    //nop
}

void
LabelSystem::initialize(Runtime& runtime)
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
