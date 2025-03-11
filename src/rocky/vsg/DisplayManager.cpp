/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "DisplayManager.h"
#include "Application.h"
#include "MapManipulator.h"
#include "terrain/TerrainEngine.h"

#ifdef ROCKY_HAS_IMGUI
#include "imgui/ImGuiIntegration.h"
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    // utility visitor to find all views.
    struct FindViews : public vsg::Visitor {

        void apply(vsg::Object& g) override {
            g.traverse(*this);
        }
        void apply(vsg::View& view) override {
            views.push_back(vsg::ref_ptr<vsg::View>(&view));
        };

        std::vector<vsg::ref_ptr<vsg::View>> views;
    };
}

GeoPoint
ROCKY_NAMESPACE::pointAtWindowCoords(vsg::ref_ptr<vsg::View> view, int x, int y)
{
    auto terrain = find<TerrainNode>(view);
    if (!terrain)
        return {};

    vsg::LineSegmentIntersector lsi(*view->camera, x, y);
    terrain->accept(lsi);
    if (lsi.intersections.empty())
        return {};

    std::sort(lsi.intersections.begin(), lsi.intersections.end(), [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });
    return GeoPoint(terrain->engine->worldSRS, lsi.intersections[0]->worldIntersection);
}

DisplayGeoPoint
ROCKY_NAMESPACE::pointAtWindowCoords(vsg::ref_ptr<vsg::Viewer> viewer, int x, int y)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer, {});

    DisplayGeoPoint result;

    for (auto& task : viewer->recordAndSubmitTasks)
    {
        for(auto cg_iter = task->commandGraphs.rbegin(); cg_iter != task->commandGraphs.rend(); ++cg_iter)
        {
            auto& cg = *cg_iter;
            auto finder = vsg::visit<FindViews>(cg);

            for(auto i = finder.views.rbegin(); i != finder.views.rend(); ++i)
            {
                auto& view = *i;

                if (view->camera)
                {
                    const auto& vp = view->camera->getViewport();

                    if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
                    {
                        result.window = cg->window;
                        result.view = view;
                        result.point = pointAtWindowCoords(view, x, y);
                    }
                }
            }
        }
    }

    return result;
}


namespace
{
    //! True is the viewer has been "realized" (compiled at least once)
    inline bool compiled(vsg::ref_ptr<vsg::Viewer> viewer)
    {
        return viewer.valid() && viewer->compileManager.valid();
    }

    // https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/debug_utils
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data)
    {
        if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            Log()->warn(std::string(callback_data->pMessage));
        }
        else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            Log()->error(std::string(callback_data->pMessage));
        }
        return VK_FALSE;
    }

    struct CloseWindowEventHandler : public vsg::Inherit<vsg::Visitor, CloseWindowEventHandler>
    {
        CloseWindowEventHandler(rocky::Application* app) : _app(app) { }

        void apply(vsg::CloseWindowEvent& e) override
        {
            auto window = e.window.ref_ptr();
            if (window)
            {
                _app->onNextUpdate([window, this]()
                    {
                        Log()->info("Removing window...");
                        _app->displayManager->removeWindow(window);

                        if (_app->viewer->windows().empty())
                        {
                            Log()->info("All windows closed... shutting down.");
                            _app->viewer->close();
                        }
                    });

                e.handled = true;
            }
        }

        rocky::Application* _app;
    };
}



DisplayManager::DisplayManager(Application& in_app) :
    app(&in_app)
{
    initialize(in_app.context);
}

DisplayManager::DisplayManager(VSGContext& in_context)
{
    initialize(in_context);
}

void
DisplayManager::initialize(VSGContext& in_context)
{
    if (!context)
    {
        context = in_context;

        if (context && context->viewer)
        {
            // intercept the window-close event so we can remove the window from our tracking tables.
            auto& handlers = context->viewer->getEventHandlers();
            handlers.insert(handlers.begin(), CloseWindowEventHandler::create(app));
        }
    }
}

vsg::ref_ptr<vsg::Device>
DisplayManager::sharedDevice()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, {});
    return !context->viewer->windows().empty() ? context->viewer->windows().front()->getDevice() : vsg::ref_ptr<vsg::Device>{ };
}

void
DisplayManager::addWindow(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    // Share device with existing windows.
    if (window->getDevice() == nullptr)
    {
        window->setDevice(sharedDevice());
    }

    // Each window gets its own CommandGraph. We will store it here and then
    // set it up later when the frame loop starts.
    auto commandgraph = vsg::CommandGraph::create(window);
    _commandGraphByWindow[window] = commandgraph;

    bool user_provied_view = view.valid();
    vsg::ref_ptr<vsg::Camera> camera;

    if (!view && app && app->mapNode && app->mainScene)
    {        
        // make a camera based on the mapNode's SRS
        double nearFarRatio = 0.00001;
        double R = app->mapNode->mapSRS().ellipsoid().semiMajorAxis();
        double ar = (double)window->extent2D().width / (double)window->extent2D().height;

        camera = vsg::Camera::create(
            vsg::Perspective::create(30.0, ar, R * nearFarRatio, R * 20.0),
            vsg::LookAt::create(),
            vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height));

        view = vsg::View::create(camera, app->mainScene);
    }

    addViewToWindow(view, window);

    // Tell Rocky it needs to mutex-protect the terrain engine
    // now that we have more than one window.
    if (app && context->viewer->windows().size() > 1)
    {
        app->mapNode->terrainSettings().supportMultiThreadedRecord = true;
    }

    // add the new window to our viewer
    context->viewer->addWindow(window);
    context->viewer->addRecordAndSubmitTaskAndPresentation({ commandgraph });

    // install a manipulator for the new view:
    if (!user_provied_view && app)
    {
        auto manip = MapManipulator::create(app->mapNode, window, camera, app->context);
        setManipulatorForView(manip, view);
    }

    // install the debug layer if requested
    if (app && app->_debuglayer && !_debugCallbackInstalled)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

        static VkDebugUtilsMessengerEXT debug_utils_messenger;

        auto vki = window->getDevice()->getInstance();

        using PFN_vkCreateDebugUtilsMessengerEXT = VkResult(VKAPI_PTR*)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
        if (vki->getProcAddr(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessenger", "vkCreateDebugUtilsMessengerEXT"))
        {
            vkCreateDebugUtilsMessengerEXT(vki->vk(), &debug_utils_create_info, nullptr, &debug_utils_messenger);
        }

        _debugCallbackInstalled = true;
    }
}

vsg::ref_ptr<vsg::Window>
DisplayManager::addWindow(vsg::ref_ptr<vsg::WindowTraits> traits)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, {});
    ROCKY_SOFT_ASSERT_AND_RETURN(traits, {});

    // wait until the device is idle to avoid changing state while it's being used.
    if (compiled(context->viewer))
    {
        context->viewer->deviceWaitIdle();
    }

    if (app)
    {
        traits->debugLayer = app->_debuglayer;
        traits->apiDumpLayer = app->_apilayer;
        if (!app->_vsync)
        {
            traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // This will install the debug messaging callback so we can capture validation errors
    traits->instanceExtensionNames.push_back("VK_EXT_debug_utils");

    if (vsg::isExtensionSupported(VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        // This is required to use the NVIDIA barycentric extension without validation errors
        if (!traits->deviceFeatures)
        {
            traits->deviceFeatures = vsg::DeviceFeatures::create();
        }
        traits->deviceExtensionNames.push_back(VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        auto& bary = traits->deviceFeatures->get<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR>();
        bary.fragmentShaderBarycentric = true;
    }

    // share the device across all windows
    traits->device = sharedDevice();

    auto window = vsg::Window::create(traits);

    addWindow(window);

    return window;
}

void
DisplayManager::removeWindow(vsg::ref_ptr<vsg::Window> window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    // wait until the device is idle to avoid changing state while it's being used.
    context->viewer->deviceWaitIdle();

    // remove the window from the viewer
    context->viewer->removeWindow(window);

    // remove the window from our tracking tables
    auto& views = windowsAndViews[window];
    for (auto& view : views)
        _viewData.erase(view);
    _commandGraphByWindow.erase(window);
    windowsAndViews.erase(window);
}

void
DisplayManager::addViewToWindow(vsg::ref_ptr<vsg::View> view, vsg::ref_ptr<vsg::Window> window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view->camera != nullptr, void());

    // if things are already running, we need to wait on the device:
    if (compiled(context->viewer))
    {
        context->viewer->deviceWaitIdle();
    }

    // find the CG associated with this window:
    auto commandgraph = getCommandGraph(window);
    if (commandgraph)
    {
        if (view->children.empty() && app)
        {
            view->addChild(app->root);
        }

        auto rendergraph = vsg::RenderGraph::create(window, view);
        rendergraph->setClearValues({ {0.1f, 0.12f, 0.15f, 1.0f} });
        commandgraph->addChild(rendergraph);

        if (compiled(context->viewer))
        {
            compileRenderGraph(rendergraph, window);
        }

        auto& viewdata = _viewData[view];
        viewdata.parentRenderGraph = rendergraph;

        windowsAndViews[window].emplace_back(view);

        if (std::find(context->activeViewIDs.begin(), context->activeViewIDs.end(), view->viewID) == context->activeViewIDs.end())
        {
            context->activeViewIDs.emplace_back(view->viewID);
        }

        if (app)
        {
            auto manip = MapManipulator::create(app->mapNode, window, view->camera, context);
            setManipulatorForView(manip, view);
        }

#ifdef ROCKY_HAS_IMGUI

        auto contextGroup = ImGuiIntegration::addContextGroup(this, window, view);

        auto imguiContext = contextGroup->imguiContext();

        // disable the .ini file for ImGui since we don't want to save stuff for internal widgetry
        ImGui::SetCurrentContext(imguiContext);
        ImGui::GetIO().IniFilename = nullptr;

        // Next, add a node that will run the actual gui rendering callbacks (like the one
        // installed by the WidgetSystem).
        contextGroup->addChild(detail::GuiRendererDispatcher::create(imguiContext, context));

        if (app)
        {
            auto viewID = view->viewID;
            VSGContext vsgContext = context;

            // We still need to process ImGui events even if we're not rendering the frame,
            // so install this no-render function:
            auto func = [vsgContext, viewID, imguiContext]()
                {
                    ImGui::SetCurrentContext(imguiContext);
                    ImGui::NewFrame();
                    for (auto& render : vsgContext->guiRenderers)
                    {
                        render(viewID, imguiContext);
                    }
                    ImGui::EndFrame();
                };

            viewdata.guiOfflineEventProcessor = std::make_shared<std::function<void()>>(func);
            app->noRenderFunctions.emplace_front(viewdata.guiOfflineEventProcessor);
        }
#endif
    }
}

void
DisplayManager::removeView(vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // wait until the device is idle to avoid changing state while it's being used.
    context->viewer->deviceWaitIdle();

    auto window = getWindow(view);
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    auto commandgraph = getCommandGraph(window);
    ROCKY_SOFT_ASSERT_AND_RETURN(commandgraph, void());

    // find the rendergraph hosting the view:
    auto vd = _viewData.find(view);
    ROCKY_SOFT_ASSERT_AND_RETURN(vd != _viewData.end(), void());
    auto& viewData = vd->second;
    auto& rendergraph = viewData.parentRenderGraph;

#ifdef ROCKY_HAS_IMGUI
    // uninstall any gui renderer elements
    if (viewData.guiOfflineEventProcessor)
    {
        auto& c = app->noRenderFunctions;
        c.erase(std::remove(c.begin(), c.end(), viewData.guiOfflineEventProcessor), c.end());
    }

    if (viewData.guiEventHandler)
    {
        auto& c = context->viewer->getEventHandlers();
        c.erase(std::remove(c.begin(), c.end(), viewData.guiEventHandler), c.end());
    }

    if (viewData.guiContextGroup)
    {
        auto& c = rendergraph->children;
        c.erase(std::remove(c.begin(), c.end(), viewData.guiContextGroup), c.end());
    }
#endif

    // remove the rendergraph from the command graph.
    auto& rps = commandgraph->children;
    rps.erase(std::remove(rps.begin(), rps.end(), rendergraph), rps.end());

    // remove it from our tracking tables.
    _viewData.erase(view);
    auto& views = windowsAndViews[vsg::observer_ptr<vsg::Window>(window)];
    views.erase(std::remove(views.begin(), views.end(), view), views.end());

    // remove it from the active-view-ID list
    auto& ids = app->context->activeViewIDs;
    ids.erase(std::remove(ids.begin(), ids.end(), view->viewID), ids.end());
}

void
DisplayManager::refreshView(vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    auto& viewdata = _viewData[view];
    ROCKY_SOFT_ASSERT_AND_RETURN(viewdata.parentRenderGraph, void());

    // wait until the device is idle to avoid changing state while it's being used.
    context->viewer->deviceWaitIdle();

    auto vp = view->camera->getViewport();
    viewdata.parentRenderGraph->renderArea.offset.x = (std::uint32_t)vp.x;
    viewdata.parentRenderGraph->renderArea.offset.y = (std::uint32_t)vp.y;
    viewdata.parentRenderGraph->renderArea.extent.width = (std::uint32_t)vp.width;
    viewdata.parentRenderGraph->renderArea.extent.height = (std::uint32_t)vp.height;

    // rebuild the graphics pipelines to reflect new camera/view params.
    vsg::UpdateGraphicsPipelines u;
    u.context = vsg::Context::create(viewdata.parentRenderGraph->getRenderPass()->device);
    u.context->renderPass = viewdata.parentRenderGraph->getRenderPass();
    viewdata.parentRenderGraph->accept(u);
}

vsg::ref_ptr<vsg::CommandGraph>
DisplayManager::getCommandGraph(vsg::ref_ptr<vsg::Window> window)
{
    auto iter = _commandGraphByWindow.find(window);
    if (iter != _commandGraphByWindow.end())
        return iter->second;
    else
        return {};
}

vsg::ref_ptr<vsg::RenderGraph>
DisplayManager::getRenderGraph(vsg::ref_ptr<vsg::View> view)
{
    auto i = _viewData.find(view);
    if (i != _viewData.end())
        return i->second.parentRenderGraph;
    else
        return { };
}

vsg::ref_ptr<vsg::Window>
DisplayManager::getWindow(vsg::ref_ptr<vsg::View> view)
{
    for (auto iter : windowsAndViews)
    {
        for (auto& a_view : iter.second)
        {
            if (a_view == view)
            {
                return iter.first;
                break;
            }
        }
    }
    return {};
}

void
DisplayManager::setManipulatorForView(vsg::ref_ptr<MapManipulator> manip, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(manip, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // stow this away in the view object so it's easy to find later.
    manip->put(view);

    // The manipulators (one for each view) need to be in the right order (top to bottom)
    // so that overlapping views don't get mixed up. To accomplish this we'll just
    // remove them all and re-insert them in the new proper order:
    auto& ehs = context->viewer->getEventHandlers();

    // remove all the MapManipulators using the dumb remove-erase idiom
    ehs.erase(
        std::remove_if(
            ehs.begin(), ehs.end(),
            [](const vsg::ref_ptr<vsg::Visitor>& v) { return dynamic_cast<MapManipulator*>(v.get()); }),
        ehs.end()
    );

    // re-add them in the right order (last to first)
    for (auto& window : windowsAndViews)
    {
        for (auto vi = window.second.rbegin(); vi != window.second.rend(); ++vi)
        {
            auto& view = *vi;
            auto manip = MapManipulator::get(view);
            if (manip)
                ehs.push_back(manip);
        }
    }
}

vsg::ref_ptr<vsg::View>
DisplayManager::getView(vsg::ref_ptr<vsg::Window> window, double x, double y)
{
    auto i = windowsAndViews.find(window);
    if (i != windowsAndViews.end())
    {
        auto& views = i->second;
        for(auto j = views.rbegin(); j != views.rend(); ++j)
        {
            auto& view = *j;
            auto vp = view->camera->getViewport();
            if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
            {
                return view;
            }
        }
    }
    return {};
}


// Call this when adding a new rendergraph to the scene.
void
DisplayManager::compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph> renderGraph, vsg::ref_ptr<vsg::Window> window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(context && context->viewer, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(renderGraph, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    vsg::ref_ptr<vsg::View> view;

    if (!renderGraph->children.empty())
    {
        view = renderGraph->children[0].cast<vsg::View>();
    }

    if (view)
    {
        // add this rendergraph's view to the viewer's compile manager.
        context->viewer->compileManager->add(*window, view);

        // Compile the new render pass for this view.
        // The lambda idiom is taken from vsgexamples/dynamicviews
        auto result = context->viewer->compileManager->compile(renderGraph, [&view](vsg::Context& context)
            {
                return context.view == view.get();
            });

        // if something was compiled, we need to update the viewer:
        if (result.requiresViewerUpdate())
        {
            vsg::updateViewer(*context->viewer, result);
        }
    }
}
