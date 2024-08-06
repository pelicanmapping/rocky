/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <vsg/utils/ShaderSet.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/state/ViewDependentState.h>

namespace ROCKY_NAMESPACE
{
    // Shader binding set and binding points for VSG's view-dependent data.
    // See vsg::ViewDependentState
    constexpr int VSG_VIEW_DEPENDENT_DATA_SET = 1;
    constexpr int VSG_VIEW_DEPENDENT_LIGHTS_BINDING = 0;
    constexpr int VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING = 1;
    

    //! Utilities for helping to set up a graphics pipeline.
    struct PipelineUtils
    {
        static void addViewDependentData(
            vsg::ref_ptr<vsg::ShaderSet> shaderSet,
            VkShaderStageFlags stageFlags)
        {
            // override it because we're getting weird VK errors -gw
            stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            // VSG view-dependent data. You must include it all even if you only intent to use
            // one of the uniforms.
            shaderSet->customDescriptorSetBindings.push_back(
                vsg::ViewDependentStateBinding::create(VSG_VIEW_DEPENDENT_DATA_SET));

            shaderSet->addDescriptorBinding(
                "vsg_lights", "",
                VSG_VIEW_DEPENDENT_DATA_SET,
                VSG_VIEW_DEPENDENT_LIGHTS_BINDING,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                stageFlags, {});

            // VSG viewport state
            shaderSet->addDescriptorBinding(
                "vsg_viewports", "",
                VSG_VIEW_DEPENDENT_DATA_SET,
                VSG_VIEW_DEPENDENT_VIEWPORTS_BINDING,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                stageFlags, {});

            //shaderSet->defines
        }

        static void enableViewDependentData(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig)
        {
            pipelineConfig->enableDescriptor("vsg_lights");
            pipelineConfig->enableDescriptor("vsg_viewports");
        }

        static vsg::ref_ptr<vsg::StateCommand> createViewDependentBindCommand(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> pipelineConfig)
        {
            return vsg::BindViewDescriptorSets::create(
                VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineConfig->layout,
                VSG_VIEW_DEPENDENT_DATA_SET);
        }
    };
}
