/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "ImGuiIntegration.h"
#include <imgui_internal.h>

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
