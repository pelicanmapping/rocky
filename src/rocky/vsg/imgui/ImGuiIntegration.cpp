/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ImGuiIntegration.h"
#include <rocky/vsg/Application.h>

using namespace ROCKY_NAMESPACE;


vsg::ref_ptr<ImGuiContextGroup> addImGuiContextGroup(
    vsg::ref_ptr<vsg::Viewer> viewer,
    vsg::ref_ptr<vsg::Window> window,
    vsg::ref_ptr<vsg::RenderGraph> renderGraph,    
    VSGContext& vsgContext)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer && window && renderGraph, {});

    // create the manager and add it to the proper render graph:
    auto group = ImGuiContextGroup::create(window);
    renderGraph->addChild(group);

    // add the event handler that will pass events from VSG to ImGui:
    auto& handlers = viewer->getEventHandlers();
    handlers.insert(handlers.begin(), SendEventsToImGuiWrapper::create(window, group->imguiContext(), vsgContext));

    return group;
}

void
ImGuiContextGroup::add(vsg::ref_ptr<ImGuiNode> node, Application& app)
{
    renderImGui->addChild(node);

    auto c = imguiContext();

    auto no_render_function = [c, node]()
        {
            ImGui::SetCurrentContext(c);
            ImGui::NewFrame();
            node->render(c);
            ImGui::EndFrame();
        };

    app.noRenderFunctions.emplace_back(std::make_shared<std::function<void()>>(no_render_function));
}
