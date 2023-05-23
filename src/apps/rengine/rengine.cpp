/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

/**
* RENGINE is an example application that demonstrates how to render 
* a Rocky MapNode using the low-level engine API directly.
* 
* Normally you will use the Application API, but if you need finer
* control you can do something like this.
*/

#include <rocky/Instance.h>
#include <rocky/Version.h>
#include <rocky/ImageLayer.h>
#include <rocky/Ephemeris.h>

#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/MapManipulator.h>
#include <rocky_vsg/SkyNode.h>

#include <vsg/all.h>
#include <chrono>

#ifdef ROCKY_SUPPORTS_GDAL
#include <rocky/GDALImageLayer.h>
#include <rocky/GDALElevationLayer.h>
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

template<class T>
int error(T layer)
{
    rocky::Log::warn() << "Problem with layer \"" <<
        layer->name() << "\" : " << layer->status().message << std::endl;
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

    // Application instance
    rocky::InstanceVSG ri(arguments);

    rocky::Log::level = rocky::LogLevel::INFO;
    rocky::Log::info() << "Hello, world." << std::endl;
    rocky::Log::info() << "Welcome to " << ROCKY_PROJECT_NAME << " version " << ROCKY_VERSION_STRING << std::endl;
    rocky::Log::info() << "Using VSG " << VSG_VERSION_STRING << " (so " << VSG_SOVERSION_STRING << ")" << std::endl;

    // An LRU cache mainly used for network data fetches.
    ri.ioOptions().services().contentCache->setCapacity(128);

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

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // TODO: read this from an earth file
    auto mapNode = rocky::MapNode::create(ri);

    // Configure the mapnode to our liking:
    mapNode->terrainSettings().concurrency = 4u;
    mapNode->terrainSettings().skirtRatio = 0.025f;
    mapNode->terrainSettings().minLevelOfDetail = 1;
    mapNode->terrainSettings().screenSpaceError = 135.0f;

    // Set up the runtime context with everything we need.
    ri.runtime().viewer = viewer;
    ri.runtime().sharedObjects = vsg::SharedObjects::create();

    if (arguments.read({ "--wire" }))
        ri.runtime().shaderCompileSettings->defines.insert("RK_WIREFRAME_OVERLAY");

#if defined(ROCKY_SUPPORTS_TMS)

    auto imagery = rocky::TMSImageLayer::create();
    imagery->setURI("https://readymap.org/readymap/tiles/1.0.0/135/");
    mapNode->map()->layers().add(imagery);
    if (imagery->status().failed())
        return error(imagery);

    auto elevation = rocky::TMSElevationLayer::create();
    elevation->setEncoding(rocky::ElevationLayer::Encoding::MapboxRGB);
    elevation->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
    mapNode->map()->layers().add(elevation);
    if (elevation->status().failed())
        return error(elevation);

#else

    auto layer = rocky::TestLayer::create();
    mapNode->map()->addLayer(layer);
    if (layer->status().failed())
        return error(layer);
#endif

    // the sun
    if (arguments.read({ "--sky" }))
    {
        auto sky = rocky::SkyNode::create(ri);
        vsg_scene->addChild(sky);
    }

    vsg_scene->addChild(mapNode);

    // main camera
    double nearFarRatio = 0.00001;
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

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera));

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
    bool measureFrameTime = (rocky::Log::level >= rocky::LogLevel::INFO);

    // rendering main loop
    auto start = std::chrono::steady_clock::now();
    while (viewer->advanceToNextFrame())
    {        
        viewer->handleEvents();

        // since an event handler could deactivate the viewer:
        if (!viewer->active())
            break;

        // rocky update pass - management of tiles and paged data
        mapNode->update(viewer->getFrameStamp());

        // runs through the viewer's update operations queue; this includes update ops 
        // initialized by rocky (tile merges for example)
        viewer->update();

        viewer->recordAndSubmit();
        viewer->present();

        frames += 1.0f;
    }

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

