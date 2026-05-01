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

namespace
{
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
        _display->vsgcontext->compileRenderGraph(renderGraph, vsgWindow);
    }

    // add this to the list of active IDs:
    auto& activeIDs = vsgcontext->activeViewIDs;
    if (std::find(activeIDs.begin(), activeIDs.end(), vsgView->viewID) == activeIDs.end())
    {
        vsgcontext->activeViewIDs.emplace_back(vsgView->viewID);
    }

    // save in our views collection
    auto& view = _views->_container.emplace_front(View(vsgView, renderGraph, _display));

    // reasonable shadowing defaults
    vsgView->viewDependentState->maxShadowDistance = 1000.0;

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
            vsgcontext->compileRenderGraph(renderGraph, vsgWindow);
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
DisplayManager::removeWindow(Window& window)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(window, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(vsgcontext && vsgcontext->viewer(), void());

    // remove all the views first:
    while (!window.views().empty())
    {
        window.removeView(window.views().front());
    }

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
