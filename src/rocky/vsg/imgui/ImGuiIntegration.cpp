/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ImGuiIntegration.h"
#include <rocky/vsg/Application.h>
#include <rocky/vsg/DisplayManager.h>

using namespace ROCKY_NAMESPACE;

ROCKY_ABOUT(imgui, IMGUI_VERSION)


vsg::ref_ptr<ImGuiContextGroup>
ROCKY_NAMESPACE::ImGuiIntegration::addContextGroup(
    vsg::ref_ptr<vsg::Viewer> viewer,
    vsg::ref_ptr<vsg::Window> window,
    vsg::ref_ptr<vsg::RenderGraph> renderGraph,    
    VSGContext vsgContext)
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

vsg::ref_ptr<ImGuiContextGroup>
ROCKY_NAMESPACE::ImGuiIntegration::addContextGroup(
    DisplayManager& display,
    vsg::ref_ptr<vsg::Window> window,
    vsg::ref_ptr<vsg::View> view)
{
    if (window == nullptr)
    {
        window = display.vsgcontext->viewer->windows().front();
    }
    ROCKY_SOFT_ASSERT_AND_RETURN(window, {});

    if (view == nullptr)
    {
        view = display.windowsAndViews[window].front();
    }
    ROCKY_SOFT_ASSERT_AND_RETURN(view, {});

    // create the manager and add it to the proper render graph:
    auto contextGroup = ImGuiContextGroup::create(window);

    ROCKY_SOFT_ASSERT_AND_RETURN(contextGroup->imguiContext(), {});

    auto& viewData = display._viewData[view];
    viewData.guiContextGroup = contextGroup;
    viewData.parentRenderGraph->addChild(contextGroup);

    // add the event handler that will pass events from VSG to ImGui:
    viewData.guiEventHandler = SendEventsToImGuiWrapper::create(window, contextGroup->imguiContext(), display.vsgcontext);
    auto& handlers = display.vsgcontext->viewer->getEventHandlers();
    handlers.insert(handlers.begin(), viewData.guiEventHandler);

    return contextGroup;
}

void
ImGuiContextGroup::add(vsg::ref_ptr<ImGuiNode> node, Application& app)
{
    addChild(node);

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
