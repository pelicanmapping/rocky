#include <rocky/Instance.h>
#include <rocky/GDALLayers.h>
#include <rocky_vsg/InstanceVSG.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/TerrainNode.h>
#include <rocky_vsg/MapManipulator.h>
#include <vsg/all.h>
#include <chrono>

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
            auto image = io.services().readImage("D:/data/images/BENDER.png", io);

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

    rk->log().threshold = rocky::LogThreshold::INFO;
    rk->log().notice << "Hello, world." << std::endl;

    // main window
    auto traits = vsg::WindowTraits::create("Rocky * Pelican Mapping");
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

    // set up the runtime context with everything we need.
    rk->runtime().compiler = [viewer]() { return viewer->compileManager; };
    rk->runtime().updates = [viewer]() { return viewer->updateOperations; };
    rk->runtime().sharedObjects = vsg::SharedObjects::create();
    rk->runtime().loaders = vsg::OperationThreads::create(mapNode->terrainNode()->concurrency);

#ifdef GDAL_FOUND
    auto layer = rocky::GDALImageLayer::create();
    layer->setURI("D:/data/imagery/world.tif");
    //layer->setURI("D:/data/naturalearth/raster-10m/HYP_HR/HYP_HR.tif");
    mapNode->map()->addLayer(layer);
#else
    auto layer = rocky::TestLayer::create();
    mapNode->map()->addLayer(layer);
#endif

    if (layer->status().failed())
    {
        rk->log().warn << "Failed to open layer: " << layer->status().message << std::endl;
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

    // prepare the scene graph
    auto commandGraph = vsg::createCommandGraphForView(
        window, 
        camera,
        vsg_scene,
        VK_SUBPASS_CONTENTS_INLINE,
        false); // assignHeadlight

    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });
    viewer->compile();


    float frames = 0.0f;
    bool measureFrameTime = (rk->log().threshold <= rocky::LogThreshold::INFO);

    // rendering main loop
    auto start = std::chrono::steady_clock::now();
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();

        mapNode->update(viewer->getFrameStamp());

        viewer->recordAndSubmit();
        viewer->present();

        frames += 1.0f;
    }
    auto end = std::chrono::steady_clock::now();

    if (measureFrameTime)
    {
        auto ms = 0.001f * (float)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        rk->log().info
            << "frames = " << frames << ", "
            << std::setprecision(3) << "ms per frame = " << (ms / frames) << ", "
            << std::setprecision(6) << "frames per second = " << 1000.f * (frames / ms)
            << std::endl;
    }

    return 0;
}

