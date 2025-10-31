/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ImGuiIntegration.h"
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>

using namespace ROCKY_NAMESPACE;

ROCKY_ABOUT(imgui, IMGUI_VERSION)

void RenderImGuiContext::add(vsg::ref_ptr<ImGuiContextNode> node)
{
    addChild(node);
    onNodeAdded.fire(node);
}

void RenderImGuiContext::traverse(vsg::RecordTraversal& record) const
{
    // active the context associated with this Node, and save it in the traversal
    ImGui::SetCurrentContext(_imguiContext);
    record.setValue("imgui.context", _imguiContext);

    if (_firstFrame)
    {
        _firstFrame = false;

#ifdef IMGUI_HAS_DOCK
        if (enableDocking)
        {
            // enable docking if supported by ImGui
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
#endif
    }

    Inherit::traverse(record);
}



#if 0
void ImGuiMultiContextRenderer::traverse(vsg::RecordTraversal& record) const
{
    // first assemble the draw list for each context
    for (auto& ci : _contextInfos)
    {
        ImGui::SetCurrentContext(ci.imguiContext);
        ImGui::NewFrame();
        ci.node->accept(record);
    }

    // then render each context
    for (auto& ci : _contextInfos)
    {
        ImGui::SetCurrentContext(ci.imguiContext);
        ImGui::Render();
    }

    // finally dispatch the final draw lists to vulkan
    for (auto& ci : _contextInfos)
    {
        ImGui::SetCurrentContext(ci.imguiContext);
        auto* drawData = ImGui::GetDrawData();
        if (drawData)
        {
            ImGui_ImplVulkan_RenderDrawData(drawData, *(record.getCommandBuffer()));
        }
    }
}
#endif
