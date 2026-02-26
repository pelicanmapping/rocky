/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <rocky/rocky.h>

#ifdef ROCKY_HAS_VSGXCHANGE
#include <vsgXchange/all.h>
#endif

int simple(int argc, char** argv);
int custom_window(int argc, char** argv);
int no_app(int argc, char** argv);
int model(const std::string&);
void install_debug_layer(vsg::ref_ptr<vsg::Window>);
bool debugLayer = false;

int main(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    if (args.read("--memcheck"))
    {
        std::cout << "Running memory check" << std::endl;
        auto image = rocky::Image::create(rocky::Image::R8G8B8A8_UNORM, 256, 256);
        auto data = rocky::moveImageToVSG(image);
        return 0;
    }

    if (args.read("--debug"))
        debugLayer = true;

    if (args.read("--simple"))
        return simple(argc, argv);

    if (args.read("--custom-window"))
        return custom_window(argc, argv);

    if (args.read("--no-app"))
        return no_app(argc, argv);

    std::string filename;
    if (args.read("--model", filename))
        return model(filename);

    rocky::Log()->info("Options: ");
    rocky::Log()->info("  --simple           (rocky::Application, fully automated)");
    rocky::Log()->info("  --custom-window    (rocky::Application, but create our own window, camera, and manipulator)");
    rocky::Log()->info("  --no-app           (Manage the VSG viewer and frame loop ourselves, no rocky::Application)");
    rocky::Log()->info("  --model            (Load and view a model)");
    return 0;
}


int simple(int argc, char** argv)
{
    rocky::Log()->info("Running simply");

    // make an application object.
    rocky::Application app(argc, argv);

    // add a layer to our map.
    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    // run until the user quits.
    return app.run();
}


int custom_window(int argc, char** argv)
{
    rocky::Log()->info("Running with a custom window");

    // make an application object.
    rocky::Application app(argc, argv);

    // add a layer to the map.
    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    // create a main window.
    auto traits = vsg::WindowTraits::create(1920, 1080, "window");
    auto window = vsg::Window::create(traits);

    // build a camera.
    double nearFarRatio = 0.00001;
    double R = app.mapNode->srs().ellipsoid().semiMajorAxis();

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto target = rocky::GeoPoint(rocky::SRS::WGS84, 0.1276, 51.507, 0.0).transform(rocky::SRS::ECEF);
    auto eye = glm::normalize(target) * R * 2.0;
    auto lookAt = vsg::LookAt::create(rocky::to_vsg(eye), rocky::to_vsg(target), vsg::dvec3(0, 0, 1));
    
    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(window->extent2D()));

    // create our view
    auto view = vsg::View::create(camera, app.mainScene);

    // add the window and its view to our application's display manager.
    app.display.addWindow(window, view);

    // add a map manipulator to the window.
    app.viewer->addEventHandler(vsg::Trackball::create(camera));

    // let's run continuous frames.
    app.renderContinuously = true;

    return app.run();
}


int no_app(int argc, char** argv)
{
    rocky::Log()->info("Running with a custom frame loop and no Application object");

    // make a viewer.
    auto viewer = vsg::Viewer::create();

    // make a rocky context wrapping that viewer.
    auto vsgcontext = rocky::VSGContextFactory::create(viewer);

    // make a MapNode with that context.
    auto mapNode = rocky::MapNode::create(vsgcontext);

    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    mapNode->map->add(layer);

    // build a camera.
    double nearFarRatio = 0.00001;
    double R = mapNode->srs().ellipsoid().semiMajorAxis();

    auto traits = vsg::WindowTraits::create(1920, 1080, argv[0]);
    if (debugLayer)
        traits->instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto window = vsg::Window::create(traits);
    viewer->addWindow(window);

    if (debugLayer)
        install_debug_layer(window);

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto lookAt = vsg::LookAt::create(
        vsg::dvec3(R * 10.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 1.0));

    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(window->extent2D()));

    // build a view, render graph, and command graph.
    auto view = vsg::View::create(camera, mapNode);
    auto rendergraph = vsg::RenderGraph::create(window, view);
    auto commandgraph = vsg::CommandGraph::create(window, rendergraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandgraph });

    // add some event handlers.
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, vsgcontext));

    // compile everything.
    viewer->compile();

    // run the VSG frame loop.
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}


int model(const std::string& filename)
{
    // make a viewer.
    auto viewer = vsg::Viewer::create();

#ifdef ROCKY_HAS_VSGXCHANGE
    // Adds all the readerwriters in vsgxchange to the options data.
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
#endif

    auto model = vsg::read_cast<vsg::Node>(filename, options);
    if (!model) {
        std::cerr << "Failed to load model: " << filename << std::endl;
        return -1;
    }

    vsg::ComputeBounds cb;
    model->accept(cb);

    // build a camera.
    double nearFarRatio = 0.00001;
    double R = vsg::length(cb.bounds.max);

    auto traits = vsg::WindowTraits::create(1920, 1080, "show model");
    auto window = vsg::Window::create(traits);
    viewer->addWindow(window);

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto lookAt = vsg::LookAt::create(
        vsg::dvec3(R * 10.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 1.0));

    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(window->extent2D()));

    // build a view, render graph, and command graph.
    auto view = vsg::View::create(camera, model);
    auto rendergraph = vsg::RenderGraph::create(window, view);
    auto commandgraph = vsg::CommandGraph::create(window, rendergraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandgraph });

    // add some event handlers.
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    view->addChild(vsg::createHeadlight());
    auto ambient = vsg::AmbientLight::create();
    ambient->color = { 0.03f, 0.03f, 0.03f };
    view->addChild(ambient);

    // compile everything.
    viewer->compile();

    // run the VSG frame loop.
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
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
        rocky::Log()->warn(std::string_view(callback_data->pMessage));
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        rocky::Log()->error(std::string_view(callback_data->pMessage));
    }
    return VK_FALSE;
}


void install_debug_layer(vsg::ref_ptr<vsg::Window> window)
{
    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

    static VkDebugUtilsMessengerEXT debug_utils_messenger;

    auto vki = window->getOrCreateDevice()->getInstance();

    using PFN_vkCreateDebugUtilsMessengerEXT = VkResult(VKAPI_PTR*)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    if (vki->getProcAddr(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessenger", "vkCreateDebugUtilsMessengerEXT"))
    {
        vkCreateDebugUtilsMessengerEXT(vki->vk(), &debug_utils_create_info, nullptr, &debug_utils_messenger);
    }
}