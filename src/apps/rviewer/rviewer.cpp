/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky/Instance.h>
#include <rocky/Version.h>
#include <rocky/ImageLayer.h>

#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/TerrainNode.h>
#include <rocky_vsg/MapManipulator.h>

#include <vsg/all.h>
#include <chrono>

#ifdef ROCKY_SUPPORTS_GDAL
#include <rocky/GDALImageLayer.h>
#endif

#ifdef ROCKY_SUPPORTS_TMS
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#endif


int usage(const char* msg)
{
    std::cout << msg << std::endl;
    return -1;
}

namespace ROCKY_NAMESPACE
{
    //! Simplest possible image layer.
    struct TestLayer : public Inherit<ImageLayer, TestLayer>
    {
        Result<GeoImage> createImageImplementation(
            const TileKey& key,
            const IOOptions& io) const override
        {
            auto image = io.services().readImageFromURI("D:/data/images/BENDER.png", io);

            if (image.status.ok())
                return GeoImage(image.value, key.extent());
            else
                return image.status;
        }
    };
}

int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    if (arguments.read({ "--help" }))
        return usage(argv[0]);

    // rocky instance
    auto rk = rocky::InstanceVSG::create(arguments);

    rocky::Log::level = rocky::LogLevel::INFO;
    rocky::Log::info() << "Hello, world." << std::endl;
    rocky::Log::info() << "Welcome to " << ROCKY_PROJECT_NAME << " version " << ROCKY_VERSION_STRING << std::endl;

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

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // TODO: read this from an earth file
    auto mapNode = rocky::MapNode::create(rk);

    mapNode->terrainNode()->concurrency = 4u;

    // Set up the runtime context with everything we need.
    // Eventually this should be automatic in InstanceVSG
    rk->runtime().compiler = [viewer]() { return viewer->compileManager; };
    rk->runtime().updates = [viewer]() { return viewer->updateOperations; };
    rk->runtime().sharedObjects = vsg::SharedObjects::create();
    rk->runtime().loaders = vsg::OperationThreads::create(mapNode->terrainNode()->concurrency);

#if defined(ROCKY_SUPPORTS_TMS)

    // add a layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->setURI("https://readymap.org/readymap/tiles/1.0.0/7/");
    mapNode->map()->addLayer(layer);
    if (layer->status().failed()) {
        rocky::Log::warn() << "Problem with layer: " << layer->status().message << std::endl;
        exit(-1);
    }

    auto elev = rocky::TMSElevationLayer::create();
    elev->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
    mapNode->map()->addLayer(elev);
    if (elev->status().failed()) {
        rocky::Log::warn() << "Problem with layer: " << elev->status().message << std::endl;
        exit(-1);
    }

#elif defined(ROCKY_SUPPORTS_GDAL)

    auto layer = rocky::GDALImageLayer::create();
    layer->setURI("D:/data/imagery/world.tif");
    mapNode->map()->addLayer(layer);

#else

    auto layer = rocky::TestLayer::create();
    mapNode->map()->addLayer(layer);

#endif

    if (layer->status().failed())
    {
        rocky::Log::warn() << "Problem with layer: " << layer->status().message << std::endl;
        exit(-1);
    }

    vsg_scene->addChild(mapNode);

    // main camera
    double nearFarRatio = 0.0005;
    double R = mapNode->mapSRS().ellipsoid().semiMajorAxis();

    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        R * nearFarRatio,
        R * 10.0);

    auto camera = vsg::Camera::create(
        perspective,
        vsg::LookAt::create(),
        vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, camera));

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
    resourceHints->numDescriptorSets = 256;
    resourceHints->descriptorPoolSizes.push_back(
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 });

    // configure the viewers rendering backend, initialize and compile Vulkan objects,
    // passing in ResourceHints to guide the resources allocated.
    viewer->compile(resourceHints);


    float frames = 0.0f;
    bool measureFrameTime = (rocky::Log::level >= rocky::LogLevel::INFO);

    // rendering main loop
    auto start = std::chrono::steady_clock::now();
    while (viewer->advanceToNextFrame())
    {        
        viewer->handleEvents();

        // since an event handler could deactivate the viewer:
        if (!viewer->active())
            break;

        mapNode->update(viewer->getFrameStamp());

        // runs through the viewer's update operations queue; this includes update ops 
        // initialized by rocky (tile merges for example)
        viewer->update();

        viewer->recordAndSubmit();
        viewer->present();

        frames += 1.0f;
    }

    viewer->stopThreading();

    auto end = std::chrono::steady_clock::now();

    if (measureFrameTime)
    {
        auto ms = 0.001f * (float)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        rocky::Log::info()
            << "frames = " << frames << ", "
            << std::setprecision(3) << "ms per frame = " << (ms / frames) << ", "
            << std::setprecision(6) << "frames per second = " << 1000.f * (frames / ms)
            << std::endl;
    }

    return 0;
}

