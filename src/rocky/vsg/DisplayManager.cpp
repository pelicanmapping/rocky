/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "DisplayManager.h"
#include "MapManipulator.h"

#include <vsg/vk/Instance.h>

#ifdef ROCKY_HAS_IMGUI
#include "imgui/ImGuiIntegration.h"
#endif

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    Window s_nullWindow;
    View s_nullView;
}

#if 0
vsg::View*
ROCKY_NAMESPACE::viewAtWindowCoords(vsg::Viewer* viewer, int x, int y)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer, nullptr);

    for (auto& task : viewer->recordAndSubmitTasks)
    {
        for (auto cg_iter = task->commandGraphs.rbegin(); cg_iter != task->commandGraphs.rend(); ++cg_iter)
        {
            auto& cg = *cg_iter;
            auto finder = vsg::visit<FindViews>(cg);

            for (auto i = finder.views.rbegin(); i != finder.views.rend(); ++i)
            {
                auto& view = *i;

                if (view->camera)
                {
                    const auto vp = view->camera->getViewport();

                    if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
                    {
                        return view;
                    }
                }
            }
        }
    }

    return nullptr;
}
#endif

namespace
{
    //bool s_debugCallbackMessagesUnique = false;
    //std::set<std::string> s_uniqueMessages;

    //! True is the viewer has been "realized" (compiled at least once)
    inline bool isCompiled(vsg::Viewer* viewer)
    {
        return viewer && viewer->compileManager.valid();
    }


    bool s_debugCallbackMessagesUnique = false;
    std::set<std::string> s_uniqueDebugMessages;


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
            if (s_uniqueDebugMessages.emplace(str).second == false)
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
}



View::View(vsg::ref_ptr<vsg::View> v, vsg::ref_ptr<vsg::RenderGraph> rg, DisplayManager* dm) :
    viewID(v ? v->viewID : 0),
    vsgView(v),
    renderGraph(rg),
    _display(dm)
{
    // todo
}

#if 0
Result<GeoPoint>
View::pointAtCoords(vsg::Node* node, int x, int y) const
{
    auto terrain = find<TerrainNode>(view);
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
#endif

void
View::dirty()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(*this, void());

    auto vsgcontext = _display->vsgcontext;
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());

    // wait until the device is idle to avoid changing state while it's being used.
    vsgcontext->viewer()->deviceWaitIdle();

    // refresh the viewport:
    auto vp = vsgView->camera->getViewport();
    renderGraph->renderArea.offset.x = (std::uint32_t)vp.x;
    renderGraph->renderArea.offset.y = (std::uint32_t)vp.y;
    renderGraph->renderArea.extent.width = (std::uint32_t)vp.width;
    renderGraph->renderArea.extent.height = (std::uint32_t)vp.height;

    // rebuild the graphics pipelines to reflect new camera/view params.
    vsg::UpdateGraphicsPipelines u;
    u.context = vsg::Context::create(renderGraph->getRenderPass()->device);
    u.context->renderPass = renderGraph->getRenderPass();
    renderGraph->accept(u);
}




Window::Window(vsg::ref_ptr<vsg::Window> w, vsg::ref_ptr<vsg::CommandGraph> cg, DisplayManager* dm) :
    vsgWindow(w),
    commandGraph(cg),
    _display(dm)
{
    //nop
}

View&
Window::addView(vsg::ref_ptr<vsg::View> vsgView)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgView && vsgView->camera && this->operator bool(), s_nullView);

    auto& vsgcontext = _display->vsgcontext;
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext, s_nullView);

    // rendergraph to control the view within a window:
    auto renderGraph = vsg::RenderGraph::create(vsgWindow, vsgView);
    renderGraph->setClearValues({ {0.0f, 0.0f, 0.0f, 1.0f} });
    commandGraph->addChild(renderGraph);

    // compile it if necessary:
    if (isCompiled(vsgcontext->viewer()))
    {
        _display->compileRenderGraph(renderGraph, vsgWindow);
    }

    // add this to the list of active IDs:
    auto& activeIDs = vsgcontext->activeViewIDs;
    if (std::find(activeIDs.begin(), activeIDs.end(), vsgView->viewID) == activeIDs.end())
    {
        vsgcontext->activeViewIDs.emplace_back(vsgView->viewID);
    }

    // save in our views collection
    auto& view = _views->_container.emplace_front(View(vsgView, renderGraph, _display));

    // fire the callback
    _display->onAddView.fire(*this, view);

    return view;
}


View&
Window::addView(vsg::ref_ptr<vsg::Camera> camera, vsg::ref_ptr<vsg::Node> node)
{
    return addView(vsg::View::create(camera, node));
}


void
Window::removeView(View& view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(*this, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    auto vsgcontext = _display->vsgcontext;
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());

    auto iter = _views->find(view);
    if (iter != _views->end())
    {
        // wait until the device is idle to avoid changing state while it's being used.
        vsgcontext->viewer()->deviceWaitIdle();

        // callback before we actually do anything:
        _display->onRemoveView.fire(*this, view);

        // remove the rendergraph from the command graph
        auto& rps = commandGraph->children;
        rps.erase(std::remove(rps.begin(), rps.end(), view.renderGraph), rps.end());

        // remove it from the active-view-ID list
        auto& ids = vsgcontext->activeViewIDs;
        ids.erase(std::remove(ids.begin(), ids.end(), view.viewID), ids.end());

        // remove it from our tracking tables
        _views->_container.erase(iter);
    }
}



View&
Window::viewAtCoords(float x, float y)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(*this, s_nullView);

    for(auto& view : views())
    {
        auto vp = view.vsgView->camera->getViewport();
        if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
        {
            return view;
        }
    }

    return s_nullView;
}

#if 0
void
DisplayManager::removeImGuiWidgetSupport(View& view) const
{
#ifdef ROCKY_HAS_IMGUI

    if (view._guiIdleEventProcessor && _app)
    {
        auto& c = _app->idleFunctions;
        c.erase(std::remove(c.begin(), c.end(), view._guiIdleEventProcessor), c.end());
    }

    if (view._guiEventVisitor)
    {
        auto& c = vsgcontext->viewer()->getEventHandlers();
        c.erase(std::remove(c.begin(), c.end(), view._guiEventVisitor), c.end());
    }

    if (view._guiContextGroup)
    {
        auto& c = view.renderGraph->children;
        c.erase(std::remove(c.begin(), c.end(), view._guiContextGroup), c.end());
    }

#endif
}
#endif

#if 0
void
DisplayManager::initialize(Application& app)
{
    _app = &app;

    initialize(app.vsgcontext);

    if (app.vsgcontext && app.vsgcontext->viewer())
    {
        // intercept the window-close event so we can remove the window from our tracking tables.
        auto& handlers = vsgcontext->viewer()->getEventHandlers();
        handlers.insert(handlers.begin(), CloseWindowEventHandler::create(_app));
    }
}
#endif


void
DisplayManager::initialize(VSGContext in_context)
{
    vsgcontext = in_context;
}

void
DisplayManager::initialize(VSGContext in_context, vsg::CommandLine& commandLine)
{
    initialize(in_context);

    _debuglayerUnique = commandLine.read("--debug-once");
    _debuglayer = _debuglayerUnique || commandLine.read("--debug");
    _apilayer = commandLine.read("--api");
    _vsync = !commandLine.read({ "--novsync", "--no-vsync" });
}


DisplayManager::~DisplayManager()
{
    // nop
}


vsg::ref_ptr<vsg::Device>
DisplayManager::sharedDevice() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), {});
    return !vsgcontext->viewer()->windows().empty() ? vsgcontext->viewer()->windows().front()->getDevice() : vsg::ref_ptr<vsg::Device>{ };
}


#if 0
void
DisplayManager::install(Window& window, View& view) //vsg::ref_ptr<vsg::Window> window, vsg::ref_ptr<vsg::View> view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window._commandGraph, void());

    // Each window gets its own CommandGraph. We will store it here and then
    // set it up later when the frame loop starts.
    window._commandGraph = vsg::CommandGraph::create(window.vsgWindow());
    //_commandGraphByWindow[window] = commandgraph;

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
#endif


void
DisplayManager::configureTraits(vsg::WindowTraits* traits)
{
    ROCKY_HARD_ASSERT(traits);

    traits->debugLayer = _debuglayer;
    traits->apiDumpLayer = _apilayer;
    if (!_vsync)
    {
        traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
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
    vsg::ref_ptr<vsg::PhysicalDevice> pd;

    if (traits->device)
    {
        pd = traits->device->getPhysicalDevice();
    }
    else
    {
        // if the device doesn't yet exist, create a temporary instance to query physical device features.
        traits->validate();

        tempInstance = vsg::Instance::create(
            traits->instanceExtensionNames,
            traits->requestedLayers,
            traits->vulkanVersion);

        pd = tempInstance->getPhysicalDevice(traits->queueFlags, traits->deviceTypePreferences);
    }

    if (pd && pd->vk() != VK_NULL_HANDLE)
    {
        // Query features from existing device's physical device
        auto supportedDS3 = pd->getFeatures<
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

    // install extensions:
    //auto pd = window._vsgWindow->getOrCreatePhysicalDevice();

    bool loadedAllRequiredExtensions = true;

    // This will install the debug messaging callback so we can capture validation errors
    if (vsg::isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        traits->instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Barycentric coordinates support for wireOverlay rendering
    if (pd->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
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
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }
}


Window&
DisplayManager::addWindow(vsg::ref_ptr<vsg::Window> vsgWindow, vsg::ref_ptr<vsg::View> vsgView)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgWindow, s_nullWindow);
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), s_nullWindow);

    // wait until the device is idle to avoid changing state while it's being used.
    if (isCompiled(vsgcontext->viewer()))
    {
        vsgcontext->viewer()->deviceWaitIdle();
    }

    // each window gets its own command graph:
    auto commandGraph = vsg::CommandGraph::create(vsgWindow);

    // add to our tracking collection:
    auto& window = _windows.emplace_back(Window(vsgWindow, commandGraph, this));

    if (vsgView)
    {
        // rendergraph to control the view within a window:
        auto renderGraph = vsg::RenderGraph::create(vsgWindow, vsgView);
        renderGraph->setClearValues({ {0.0f, 0.0f, 0.0f, 1.0f} });
        commandGraph->addChild(renderGraph);

        // compile it if necessary:
        if (isCompiled(vsgcontext->viewer()))
        {
            compileRenderGraph(renderGraph, vsgWindow);
        }

        // add this to the list of active IDs:
        auto& activeIDs = vsgcontext->activeViewIDs;
        if (std::find(activeIDs.begin(), activeIDs.end(), vsgView->viewID) == activeIDs.end())
        {
            vsgcontext->activeViewIDs.emplace_back(vsgView->viewID);
        }

        // save in our views collection
        auto& view = window.views()._container.emplace_front(View(vsgView, renderGraph, this));

        onAddWindow.fire(window);
        onAddView.fire(window, view);
    }

    else
    {
        onAddWindow.fire(window);
    }

    return window;
}


Window&
DisplayManager::addWindow(vsg::ref_ptr<vsg::WindowTraits> traits)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(traits, s_nullWindow);
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), s_nullWindow);

    // wait until the device is idle to avoid changing state while it's being used.
    if (isCompiled(vsgcontext->viewer()))
    {
        vsgcontext->viewer()->deviceWaitIdle();
    }

    // add rocky-specific requirements to the traits:
    configureTraits(traits.get());

    // make a new VSG window:
    auto vsgWindow = vsg::Window::create(traits);
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgWindow, s_nullWindow);

    // each window gets its own command graph:
    auto commandGraph = vsg::CommandGraph::create(vsgWindow);

    // add to our tracking collection:
    auto& window = _windows.emplace_back(Window(vsgWindow, commandGraph, this));

    onAddWindow.fire(window);

    // install the debug layer if requested
    if (_debuglayer && !_debugCallbackInstalled)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

        static VkDebugUtilsMessengerEXT debug_utils_messenger;

        auto vki = sharedDevice()->getInstance();

        auto vkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vki->vk(), "vkCreateDebugUtilsMessengerEXT"));

        if (vkCreateDebugUtilsMessengerEXT)
        {
            Log()->info("Installed Vulkan debug callback messenger.");
            vkCreateDebugUtilsMessengerEXT(vki->vk(), &debug_utils_create_info, nullptr, &debug_utils_messenger);
        }

        _debugCallbackInstalled = true;
        s_debugCallbackMessagesUnique = _debuglayerUnique;
    }

    if (sharedDevice()->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        vsgcontext->shaderCompileSettings->defines.emplace("ROCKY_HAS_VK_BARYCENTRIC_EXTENSION");
    }

    return window;
}


#if 0
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
        Log()->debug("Enabling: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        traits->instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Barycentric coordinates support for wireOverlay rendering
    if (pd->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
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
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        traits->deviceExtensionNames.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
    }
    else
    {
        Log()->warn("Not available: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
        loadedAllRequiredExtensions = false;
    }

    if (pd->supportsDeviceExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
    {
        Log()->debug("Enabling: {}", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
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
#endif

void
DisplayManager::removeWindow(const Window& window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());

    // and remove it from our tracker:
    auto iter = std::find(_windows.begin(), _windows.end(), window);
    if (iter != _windows.end())
    {
        // fire the callback first:
        onRemoveWindow.fire(window);

        // and from our own collection:
        _windows.erase(iter);
    }
}


#if 0
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
#endif

#if 0
void
DisplayManager::addViewToWindow(View& view, Window& window, bool add_manipulator)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view.vsgView()->camera != nullptr, void());

    // if things are already running, we need to wait on the device:
    if (compiled(vsgcontext->viewer()))
    {
        vsgcontext->viewer()->deviceWaitIdle();
    }

    // find the CG associated with this window:
    auto vsgView = view.vsgView();

    if (vsgView->children.empty() && _app)
    {
        vsgView->addChild(_app->root);
    }

    view._renderGraph = vsg::RenderGraph::create(window, view);
    view._renderGraph->setClearValues({ {0.0f, 0.0f, 0.0f, 1.0f} });
    window.commandGraph()->addChild(rendergraph);

    if (compiled(vsgcontext->viewer()))
    {
        compileRenderGraph(view._renderGraph, window.vsgWindow());
    }

    window._views->emplace_back(view);

    if (std::find(vsgcontext->activeViewIDs.begin(), vsgcontext->activeViewIDs.end(), view.viewID()) == vsgcontext->activeViewIDs.end())
    {
        vsgcontext->activeViewIDs.emplace_back(view.viewID());
    }

    bool rocky_auto_view = false;
    vsgView->getValue("rocky_auto_created", rocky_auto_view);

    if (_app && (rocky_auto_view || add_manipulator))
    {
        if (auto mapNode = view.get<MapNode>())
        {
            auto manip = MapManipulator::create(mapNode, window, vsgView->camera, vsgcontext);
            setManipulatorForView(manip, vsgView);
        }
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
            auto func = [view, vsgcontext(vsgcontext), viewID(view->viewID), imguicontext]()
                {
                    auto vp = view->camera->getViewport();
                     RenderingState rs{
                         view->viewID,
                         vsgcontext->viewer()->getFrameStamp()->frameCount,
                         { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
                     };

                    ImGui::SetCurrentContext(imguicontext);
                    ImGui::GetIO().DeltaTime = ImGui::GetIO().DeltaTime <= 0.0f ? 0.016f : ImGui::GetIO().DeltaTime;
                    ImGui::NewFrame();
                    for (auto& record : vsgcontext->guiRecorders)
                    {
                        record(rs, imguicontext);
                    }
                    ImGui::EndFrame();
                };

            viewdata.guiIdleEventProcessor = std::make_shared<std::function<void()>>(func);
            _app->idleFunctions.emplace_front(viewdata.guiIdleEventProcessor);
        }
#endif
    }
}
#endif

#if 0
void
DisplayManager::addImGuiWidgetSupport(Window& window, View& view) const
{
#ifdef ROCKY_HAS_IMGUI

    // ImGui renderer for drawing Widgets (et al) on this view:
    auto imguiRenderer = RenderImGuiContext::create(window.vsgWindow, view.vsgView);
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
        auto idleFrame = [view(view), vsgcontext(vsgcontext), imguicontext]()
            {
                auto vp = view.vsgView->camera->getViewport();

                RenderingState rs{
                    view.viewID,
                    vsgcontext->viewer()->getFrameStamp()->frameCount,
                    { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
                };

                ImGui::SetCurrentContext(imguicontext);
                ImGui::GetIO().DeltaTime = ImGui::GetIO().DeltaTime <= 0.0f ? 0.016f : ImGui::GetIO().DeltaTime;
                ImGui::NewFrame();

                for (auto& record : vsgcontext->guiRecorders)
                {
                    record(rs, imguicontext);
                }

                ImGui::EndFrame();
            };

        view._guiIdleEventProcessor = std::make_shared<std::function<void()>>(idleFrame);
        _app->idleFunctions.emplace_front(view._guiIdleEventProcessor);
    }
#endif
}
#endif

#if 0
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
#endif

#if 0
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
#endif

#if 0
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
#endif

void
DisplayManager::setManipulatorForView(vsg::ref_ptr<MapManipulator> manip, vsg::View* view)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());
    ROCKY_SOFT_ASSERT_AND_RETURN(manip, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(view, void());

    // stow this away in the view object so it's easy to find later.
    manip->put(view);

    // The manipulators (one for each view) need to be in the right order (top to bottom)
    // so that overlapping views don't get mixed up. To accomplish this we'll just
    // remove them all and re-insert them in the new proper order:
    auto& handlers = vsgcontext->viewer()->getEventHandlers();

    // remove all the MapManipulators using the dumb remove-erase idiom
    handlers.erase(
        std::remove_if(
            handlers.begin(), handlers.end(),
            [](const vsg::ref_ptr<vsg::Visitor>& v) { return v.cast<MapManipulator>(); }),
        handlers.end()
    );

    // re-add them in the right order (last to first)
    for (auto& window : windows())
    {
        for(auto& view : window.views())
        {
            if (auto manip = MapManipulator::get(view.vsgView))
            {
                handlers.emplace_back(manip);
            }
        }
    }
}

#if 0
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
#endif


Window&
DisplayManager::find(vsg::Window* vsgWindow)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgWindow, s_nullWindow);

    for (auto& window : windows())
    {
        if (window.vsgWindow.get() == vsgWindow)
        {
            return window;
        }
    }
    return s_nullWindow;
}


View&
DisplayManager::find(vsg::View* vsgView)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgView, s_nullView);

    for (auto& window : windows())
    {
        for (auto& view : window.views())
        {
            if (view.vsgView.get() == vsgView)
            {
                return view;
            }
        }
    }
    return s_nullView;
}


Window&
DisplayManager::window(std::size_t index)
{
    return (index < windows().size()) ? windows()[index] : s_nullWindow;
}

const Window&
DisplayManager::window(std::size_t index) const
{
    return (index < windows().size()) ? windows()[index] : s_nullWindow;
}

std::vector<Window>&
DisplayManager::windows()
{
    return _windows;
}

const std::vector<Window>&
DisplayManager::windows() const
{
    return _windows;
}


std::tuple<Window, View>
DisplayManager::windowAndViewAtCoords(float x, float y)
{
    // iterate backwards so we search top to bottom
    for (auto window_iter = windows().rbegin(); window_iter != windows().rend(); ++window_iter)
    {
        if (auto& window = *window_iter)
        {
            for (auto& view : window.views())
            {
                if (view.vsgView->camera)
                {
                    const auto vp = view.vsgView->camera->getViewport();

                    if (x >= vp.x && x < vp.x + vp.width && y >= vp.y && y < vp.y + vp.height)
                    {
                        return std::make_tuple(window, view);
                    }
                }
            }
        }
    }

    return {};
}

#if 0
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
#endif


// Call this when adding a new rendergraph to the scene.
void
DisplayManager::compileRenderGraph(vsg::ref_ptr<vsg::RenderGraph> renderGraph, vsg::ref_ptr<vsg::Window> window)
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
        auto result = vsgcontext->viewer()->compileManager->compile(renderGraph, [view](vsg::Context& compileContext)
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

//
//vsg::ref_ptr<vsg::Window>
//DisplayManager::mainWindow() const
//{
//    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), {});
//    return !vsgcontext->viewer()->windows().empty() ? vsgcontext->viewer()->windows().front() : vsg::ref_ptr<vsg::Window>{ };
//}

#if 0
Result<GeoPoint>
DisplayManager::pointAtWindowCoords(vsg::ref_ptr<vsg::Window> window, int x, int y) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window, Failure_AssertionFailure);

    for (auto view : views(window))
    {
        if (view->camera)
        {
            const auto vp = view->camera->getViewport();
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
#endif