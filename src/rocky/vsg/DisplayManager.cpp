/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "DisplayManager.h"
#include "Application.h"
#include "MapManipulator.h"
#include "terrain/TerrainEngine.h"

#include <vsg/vk/Instance.h>

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

Result<GeoPoint>
ROCKY_NAMESPACE::pointAtWindowCoords(vsg::ref_ptr<vsg::View> view, int x, int y)
{
    auto terrain = util::find<TerrainNode>(view);
    if (!terrain)
        return Failure_AssertionFailure;

    vsg::LineSegmentIntersector lsi(*view->camera, x, y);
    terrain->accept(lsi);
    if (lsi.intersections.empty())
        return Failure{};

    auto closest = std::min_element(
        lsi.intersections.begin(), lsi.intersections.end(),
        [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

    return GeoPoint(terrain->renderingSRS, closest->get()->worldIntersection);
}

Result<DisplayGeoPoint>
ROCKY_NAMESPACE::pointAtWindowCoords(vsg::ref_ptr<vsg::Viewer> viewer, int x, int y)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer, Failure_ConfigurationError);

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
                        auto point = pointAtWindowCoords(view, x, y);
                        if (point.ok())
                        {
                            result.window = cg->window;
                            result.view = view;
                            result.point = point.value();
                        }
                    }
                }
            }
        }
    }

    if (result.point.valid())
        return result;
    else
        return Failure{};
}


namespace
{
    bool s_debugCallbackMessagesUnique = false;
    std::set<std::string> s_uniqueMessages;

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
        std::string str(callback_data->pMessage);

        if (s_debugCallbackMessagesUnique)
        {
            if (s_uniqueMessages.emplace(str).second == false)
                return VK_FALSE;
        }
        if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            Log()->warn(str);
        }
        else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            Log()->error(str);
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
                        _app->display.removeWindow(window);

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

void
DisplayManager::initialize(Application& app)
{
    _app = &app;

    initialize(app.vsgcontext);

    if (vsgcontext && vsgcontext->viewer())
    {
        // intercept the window-close event so we can remove the window from our tracking tables.
        auto& handlers = vsgcontext->viewer()->getEventHandlers();
        handlers.insert(handlers.begin(), CloseWindowEventHandler::create(_app));
    }
}

void
DisplayManager::initialize(VSGContext in_context)
{
    vsgcontext = in_context;
}

vsg::ref_ptr<vsg::Device>
DisplayManager::sharedDevice() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), {});
    return !vsgcontext->viewer()->windows().empty() ? vsgcontext->viewer()->windows().front()->getDevice() : vsg::ref_ptr<vsg::Device>{ };
}

void
DisplayManager::addWindow(vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
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

    bool user_provided_view = view.valid();
    vsg::ref_ptr<vsg::Camera> camera;

    if (!view && _app && _app->mapNode && _app->mainScene)
    {        
        // make a camera based on the mapNode's SRS
        double nearFarRatio = 0.0000001;
        double R = _app->mapNode->srs().ellipsoid().semiMajorAxis();
        double ar = (double)window->extent2D().width / (double)window->extent2D().height;

        camera = vsg::Camera::create(
            vsg::Perspective::create(30.0, ar, R * nearFarRatio, R * 20.0),
            vsg::LookAt::create(vsg::dvec3(R*5.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0)),
            vsg::ViewportState::create(0, 0, window->extent2D().width, window->extent2D().height));

        view = vsg::View::create(camera, _app->mainScene);
        view->setValue("rocky_auto_created", true);
    }

    addViewToWindow(view, window, !user_provided_view);

    // add the new window to our viewer
    vsgcontext->viewer()->addWindow(window);
    vsgcontext->viewer()->addRecordAndSubmitTaskAndPresentation({ commandgraph });

    // Tell Rocky it needs to mutex-protect the terrain engine
    // now that we have more than one window.
    if (_app && vsgcontext->viewer()->windows().size() > 1)
    {
        _app->mapNode->terrainSettings().supportMultiThreadedRecord = true;
    }

    // install the debug layer if requested
    if (_app && _app->_debuglayer && !_debugCallbackInstalled)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

        static VkDebugUtilsMessengerEXT debug_utils_messenger;

        auto vki = window->getDevice()->getInstance();
        
        auto vkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vki->vk(), "vkCreateDebugUtilsMessengerEXT"));

        if (vkCreateDebugUtilsMessengerEXT)
        {
            Log()->info("Installed Vulkan debug callback messenger.");
            vkCreateDebugUtilsMessengerEXT(vki->vk(), &debug_utils_create_info, nullptr, &debug_utils_messenger);
        }

        _debugCallbackInstalled = true;
        s_debugCallbackMessagesUnique = _app->_debuglayerUnique;
    }

    if (sharedDevice()->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        vsgcontext->shaderCompileSettings->defines.emplace("ROCKY_HAS_VK_BARYCENTRIC_EXTENSION");
    }
}

vsg::ref_ptr<vsg::Window>
DisplayManager::addWindow(vsg::ref_ptr<vsg::WindowTraits> traits)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), {});
    ROCKY_SOFT_ASSERT_AND_RETURN(traits, {});

    // wait until the device is idle to avoid changing state while it's being used.
    if (compiled(vsgcontext->viewer()))
    {
        vsgcontext->viewer()->deviceWaitIdle();
    }

    if (_app)
    {
        traits->debugLayer = _app->_debuglayer;
        traits->apiDumpLayer = _app->_apilayer;
        if (!_app->_vsync)
        {
            traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // share the device across all windows
    traits->device = sharedDevice();

    // install necessary device features
    traits->deviceFeatures->get().fillModeNonSolid = VK_TRUE;

    auto& ds1 = traits->deviceFeatures->get<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT>();
    ds1.extendedDynamicState = VK_TRUE;

    auto& ds2 = traits->deviceFeatures->get<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT>();
    ds2.extendedDynamicState2 = VK_TRUE;

    // Query which extended_dynamic_state_3 features are actually supported.
    // Some Vulkan implementations (e.g., MoltenVK on macOS) don't support all features
    // of an extension even when the extension itself is reported as supported.
    auto& ds3 = traits->deviceFeatures->get<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT>();

    vsg::ref_ptr<vsg::Instance> tempInstance;
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice;
    
    if (traits->device)
    {
        physicalDevice = traits->device->getPhysicalDevice();
    }
    else
    {
        // if the device doesn't yet exist, create a temporary instance to query physical device features.
        traits->validate();

        tempInstance = vsg::Instance::create(
            traits->instanceExtensionNames,
            traits->requestedLayers,
            traits->vulkanVersion);

        physicalDevice = tempInstance->getPhysicalDevice(traits->queueFlags, traits->deviceTypePreferences);
    }

    if (physicalDevice && physicalDevice->vk() != VK_NULL_HANDLE)
    {
        // Query features from existing device's physical device
        auto supportedDS3 = physicalDevice->getFeatures<
            VkPhysicalDeviceExtendedDynamicState3FeaturesEXT,
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT>();

        if (supportedDS3.extendedDynamicState3PolygonMode)
            ds3.extendedDynamicState3PolygonMode = VK_TRUE;

        if (supportedDS3.extendedDynamicState3ColorWriteMask)
            ds3.extendedDynamicState3ColorWriteMask = VK_TRUE;
    }
    else
    {
        // Fallback: enable commonly supported feature only
        ds3.extendedDynamicState3PolygonMode = VK_TRUE;
    }

    auto window = vsg::Window::create(traits);

    // install extensions:
    auto pd = window->getOrCreatePhysicalDevice();

    bool loadedAllRequiredExtensions = true;

    // This will install the debug messaging callback so we can capture validation errors
    if (vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        Log()->info("Enabling: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        traits->instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Barycentric coordinates support for wireOverlay rendering
    if (pd->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        Log()->info("Enabling: {}", VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    // All the dynamic state extensions
    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME))
    {
        Log()->info("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
        Log()->info("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
    {
        Log()->info("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    // configure the window
    addWindow(window);

    return window;
}

void
DisplayManager::removeWindow(vsg::ref_ptr<vsg::Window> window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    // wait until the device is idle to avoid changing state while it's being used.
    vsgcontext->viewer()->deviceWaitIdle();

    // remove the window from the viewer
    vsgcontext->viewer()->removeWindow(window);

    // remove the window from our tracking tables
    auto& views = windowsAndViews[window];
    for (auto& view : views)
        _viewData.erase(view);
    _commandGraphByWindow.erase(window);
    windowsAndViews.erase(window);
}

void
DisplayManager::addViewToWindow(vsg::ref_ptr<vsg::View> view, vsg::ref_ptr<vsg::Window> window,
    bool add_manipulator)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view != nullptr, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view->camera != nullptr, void());

    // if things are already running, we need to wait on the device:
    if (compiled(vsgcontext->viewer()))
    {
        vsgcontext->viewer()->deviceWaitIdle();
    }

    // find the CG associated with this window:
    auto commandgraph = commandGraph(window);
    if (commandgraph)
    {
        if (view->children.empty() && _app)
        {
            view->addChild(_app->root);
        }

        auto rendergraph = vsg::RenderGraph::create(window, view);
        rendergraph->setClearValues({ {0.0f, 0.0f, 0.0f, 1.0f} });
        commandgraph->addChild(rendergraph);

        if (compiled(vsgcontext->viewer()))
        {
            compileRenderGraph(rendergraph, window);
        }

        auto& viewdata = _viewData[view];
        viewdata.parentRenderGraph = rendergraph;

        windowsAndViews[window].emplace_back(view);

        if (std::find(vsgcontext->activeViewIDs.begin(), vsgcontext->activeViewIDs.end(), view->viewID) == vsgcontext->activeViewIDs.end())
        {
            vsgcontext->activeViewIDs.emplace_back(view->viewID);
        }

        bool rocky_auto_view = false;
        view->getValue("rocky_auto_created", rocky_auto_view);

        if (_app && (rocky_auto_view || add_manipulator))
        {
            auto manip = MapManipulator::create(_app->mapNode, window, view->camera, vsgcontext);
            setManipulatorForView(manip, view);
        }

#ifdef ROCKY_HAS_IMGUI

        // ImGui renderer for drawing Widgets (et al) on this view:

        auto imguiRenderer = RenderImGuiContext::create(window, view);

        auto imguicontext = imguiRenderer->imguiContext();

        // disable the .ini file for ImGui since we don't want to save stuff for internal widgetry
        ImGui::SetCurrentContext(imguicontext);
        ImGui::GetIO().IniFilename = nullptr;

        // Next, add a node that will dispatch the actual gui rendering callbacks
        // (like the one installed by the WidgetSystem):
        imguiRenderer->addChild(detail::ImGuiDispatcher::create(imguicontext, vsgcontext));

        if (_app)
        {
            _app->install(imguiRenderer, false);

            // We still need to process ImGui events even if we're not rendering the frame,
            // so install this "idle" function:
            auto func = [vsgcontext(vsgcontext), viewID(view->viewID), imguicontext]()
                {
                    RenderingState vrs{
                        viewID,
                        vsgcontext->viewer()->getFrameStamp()->frameCount
                    };

                    ImGui::SetCurrentContext(imguicontext);
                    ImGui::GetIO().DeltaTime = ImGui::GetIO().DeltaTime <= 0.0f ? 0.016f : ImGui::GetIO().DeltaTime;
                    ImGui::NewFrame();
                    for (auto& record : vsgcontext->guiRecorders)
                    {
                        record(vrs, imguicontext);
                    }
                    ImGui::EndFrame();
                };

            viewdata.guiIdleEventProcessor = std::make_shared<std::function<void()>>(func);
            _app->idleFunctions.emplace_front(viewdata.guiIdleEventProcessor);
        }
#endif
    }
}

void
DisplayManager::removeView(vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // wait until the device is idle to avoid changing state while it's being used.
    vsgcontext->viewer()->deviceWaitIdle();

    auto window = windowContainingView(view);
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());

    auto commandgraph = commandGraph(window);
    ROCKY_SOFT_ASSERT_AND_RETURN(commandgraph, void());

    // find the rendergraph hosting the view:
    auto vd = _viewData.find(view);
    ROCKY_SOFT_ASSERT_AND_RETURN(vd != _viewData.end(), void());
    auto& viewData = vd->second;
    auto& rendergraph = viewData.parentRenderGraph;

#ifdef ROCKY_HAS_IMGUI
    // uninstall any gui renderer elements
    if (viewData.guiIdleEventProcessor && _app)
    {
        auto& c = _app->idleFunctions;
        c.erase(std::remove(c.begin(), c.end(), viewData.guiIdleEventProcessor), c.end());
    }

    if (viewData.guiEventVisitor)
    {
        auto& c = vsgcontext->viewer()->getEventHandlers();
        c.erase(std::remove(c.begin(), c.end(), viewData.guiEventVisitor), c.end());
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
    auto& ids = vsgcontext->activeViewIDs;
    ids.erase(std::remove(ids.begin(), ids.end(), view->viewID), ids.end());
}

void
DisplayManager::refreshView(vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    auto& viewdata = _viewData[view];
    ROCKY_SOFT_ASSERT_AND_RETURN(viewdata.parentRenderGraph, void());

    // wait until the device is idle to avoid changing state while it's being used.
    vsgcontext->viewer()->deviceWaitIdle();

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
DisplayManager::commandGraph(vsg::ref_ptr<vsg::Window> window) const
{
    auto iter = _commandGraphByWindow.find(window);
    if (iter != _commandGraphByWindow.end())
        return iter->second;
    else
        return {};
}

vsg::ref_ptr<vsg::RenderGraph>
DisplayManager::renderGraph(vsg::ref_ptr<vsg::View> view) const
{
    auto i = _viewData.find(view);
    if (i != _viewData.end())
        return i->second.parentRenderGraph;
    else
        return { };
}

vsg::ref_ptr<vsg::Window>
DisplayManager::windowContainingView(vsg::ref_ptr<vsg::View> view) const
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
DisplayManager::setManipulatorForView(vsg::ref_ptr<MapManipulator> manip, vsg::ref_ptr<vsg::View> view) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(manip, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // stow this away in the view object so it's easy to find later.
    manip->put(view);

    // The manipulators (one for each view) need to be in the right order (top to bottom)
    // so that overlapping views don't get mixed up. To accomplish this we'll just
    // remove them all and re-insert them in the new proper order:
    auto& ehs = vsgcontext->viewer()->getEventHandlers();

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

std::vector<vsg::ref_ptr<vsg::View>>
DisplayManager::views(vsg::ref_ptr<vsg::Window> window) const
{
    auto i = windowsAndViews.find(window);
    if (i != windowsAndViews.end())
    {
        return i->second;
    }
    return {};
}

vsg::ref_ptr<vsg::View>
DisplayManager::viewAtWindowCoords(vsg::ref_ptr<vsg::Window> window, double x, double y) const
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
DisplayManager::compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph> renderGraph, vsg::ref_ptr<vsg::Window> window) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
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
        vsgcontext->viewer()->compileManager->add(*window, view);

        // Compile the new render pass for this view.
        // The lambda idiom is taken from vsgexamples/dynamicviews
        auto result = vsgcontext->viewer()->compileManager->compile(renderGraph, [&view](vsg::Context& compileContext)
            {
                return compileContext.view == view.get();
            });

        // if something was compiled, we need to update the viewer:
        if (result.requiresViewerUpdate())
        {
            vsg::updateViewer(*vsgcontext->viewer(), result);
        }
    }
}

vsg::ref_ptr<vsg::Window>
DisplayManager::mainWindow() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), {});
    return !vsgcontext->viewer()->windows().empty() ? vsgcontext->viewer()->windows().front() : vsg::ref_ptr<vsg::Window>{ };
}

Result<GeoPoint>
DisplayManager::pointAtWindowCoords(vsg::ref_ptr<vsg::Window> window, int x, int y) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window, Failure_AssertionFailure);

    for (auto view : views(window))
    {
        if (view->camera)
        {
            const auto& vp = view->camera->getViewport();
            if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
            {
                auto point = ROCKY_NAMESPACE::pointAtWindowCoords(view, x, y);
                if (point.ok())
                {
                    return point;
                }
            }
        }
    }

    return Failure{ };
}