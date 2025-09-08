/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

/**
* ROCKY_ENGINE is an example application that demonstrates how to 
* use a rocky::MapNode without the rocky::Application API. This is the
* approach to use if you are managing VSG windows and views yourself
* and just want to embed Rocky maps in your own app.
*/

#include <rocky/Context.h>
#include <rocky/Version.h>
#include <rocky/ImageLayer.h>
#include <rocky/TMSImageLayer.h>

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/MapNode.h>
#include <rocky/vsg/MapManipulator.h>

#include <chrono>
#include <iomanip>

int usage(const char* msg)
{
    std::cout << msg << std::endl;
    return -1;
}

template<class T>
int error(T layer)
{
    rocky::Log()->warn("Problem with layer \"" + layer->name() + "\" : " + layer->status().message);
    return -1;
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    if (arguments.read({ "--help" }))
        return usage(argv[0]);

    rocky::Log()->set_level(rocky::log::level::info);
    rocky::Log()->info("Hello, world.");
    rocky::Log()->info("Welcome to " ROCKY_PROJECT_NAME " version " ROCKY_VERSION_STRING);
    rocky::Log()->info("Using VSG " VSG_VERSION_STRING " (so " VSG_SOVERSION_STRING ")");

    // main window
    auto traits = vsg::WindowTraits::create(ROCKY_PROJECT_NAME);
    traits->debugLayer = arguments.read({ "--debug" });
    traits->apiDumpLayer = arguments.read({ "--api" });
    traits->samples = 1;
    traits->width = 1920;
    traits->height = 1080;
    if (arguments.read({ "--novsync" }))
        traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    auto window = vsg::Window::create(traits);
    window->clearColor() = VkClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f };
    bool multithreading = arguments.read({ "--mt" });

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // Rocky runtime context connected to the viewer:
    auto context = rocky::VSGContextFactory::create(viewer);
    if (!context->status.ok()) {
        rocky::Log()->error("Cannot create rocky context: {}" + context->status.error().message);
        return -1;
    }

    // the map node - renders the terrain
    auto mapNode = rocky::MapNode::create(context);

    // Configure the terrain engine to our liking:
    mapNode->terrainSettings().concurrency = 4u;
    mapNode->terrainSettings().minLevel = 1;
    mapNode->terrainSettings().pixelError = 135.0f;

    // Add a base image layer:
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://[abc].tile.openstreetmap.org/{z}/{x}/{y}.png";
    layer->attribution = rocky::Hyperlink{ "\u00a9 OpenStreetMap contributors", "https://openstreetmap.org/copyright" };
    layer->profile = rocky::Profile("spherical-mercator");
    mapNode->map->add(layer);

    // A rendering profile (optional) that works well with OSM tiles
    mapNode->profile = rocky::Profile("global-qsc");

    // Add our map to the scene:
    vsg_scene->addChild(mapNode);


    // main camera
    double nearFarRatio = 0.00001;
    double R = mapNode->srs().ellipsoid().semiMajorAxis();

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto camera = vsg::Camera::create(
        perspective,
        vsg::LookAt::create(),
        vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, context));

    // associate the scene graph with a window and camera in a new render graph
    auto renderGraph = vsg::createRenderGraphForView(
        window,
        camera,
        vsg_scene,
        VK_SUBPASS_CONTENTS_INLINE,
        false); // assignHeadlight

    // Command graph holds the render graph:
    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->addChild(renderGraph);

    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

    // Configure a descriptor pool size that's appropriate for paged terrains
    // (they are a good candidate for DS reuse)
    // https://groups.google.com/g/vsg-users/c/JJQZ-RN7jC0/m/tyX8nT39BAAJ
    auto resourceHints = vsg::ResourceHints::create();
    resourceHints->numDescriptorSets = 1024;
    resourceHints->descriptorPoolSizes.push_back(
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 });

    // configure the viewers rendering backend, initialize and compile Vulkan objects,
    // passing in ResourceHints to guide the resources allocated.
    viewer->compile(resourceHints);

    if (multithreading)
        viewer->setupThreading();

    float frames = 0.0f;
    bool measureFrameTime = (rocky::Log()->level() >= rocky::log::level::info);

    // rendering main loop
    auto start = std::chrono::steady_clock::now();
    while (viewer->advanceToNextFrame())
    {        
        viewer->handleEvents();

        // since an event handler could deactivate the viewer:
        if (!viewer->active())
            break;

        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        frames += 1.0f;
    }

    auto end = std::chrono::steady_clock::now();

    if (measureFrameTime)
    {
        auto ms = 0.001f * (float)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::stringstream buf;
        buf << "frames = " << frames << ", "
            << std::setprecision(3) << "ms per frame = " << (ms / frames) << ", "
            << std::setprecision(6) << "frames per second = " << 1000.f * (frames / ms)
            << std::endl;
        rocky::Log()->info(buf.str());

    }

    return 0;
}

