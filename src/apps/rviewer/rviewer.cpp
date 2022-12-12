#include <rocky/Instance.h>
#include <rocky/Notify.h>
#include <rocky/GDALLayers.h>
#include <rocky_vsg/MapNode.h>
#include <rocky_vsg/TerrainNode.h>
#include <vsg/all.h>
#include <chrono>

using namespace rocky;

int usage(const char* msg)
{
    std::cout << msg << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    // rocky instance
    rocky::Instance instance;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.read({ "--help" }))
        return usage(argv[0]);

    // options that get passed to VSG reader/writer modules?
    auto options = vsg::Options::create();
    arguments.read(options);

    // window configuration
    auto traits = vsg::WindowTraits::create("Rocky * Pelican Mapping");
    traits->debugLayer = arguments.read({ "--debug" });
    traits->apiDumpLayer = arguments.read({ "--api" });
    traits->samples = 1;
    if (arguments.read({ "--novsync" }))
        traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    auto window = vsg::Window::create(traits);

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // TODO: read this from an earth file
    auto mapNode = MapNode::create(instance);

    // set up the runtime context with everything we need.
    mapNode->runtime.compiler = [viewer]() { return viewer->compileManager; };
    mapNode->runtime.updates = [viewer]() { return viewer->updateOperations; };
    mapNode->runtime.sharedObjects = vsg::SharedObjects::create();
    mapNode->runtime.loaders = vsg::OperationThreads::create(mapNode->getTerrainNode()->concurrency);

    auto layer = GDALImageLayer::create();
    layer->setURI("D:/data/imagery/world.tif");
    //layer->setURI("D:/data/naturalearth/raster-10m/HYP_HR/HYP_HR.tif");
    mapNode->getMap()->addLayer(layer);

    vsg_scene->addChild(mapNode);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds cb;
    vsg_scene->accept(cb);
    vsg::dsphere bs(
        (cb.bounds.min + cb.bounds.max) * 0.5,
        vsg::length(cb.bounds.max - cb.bounds.min) * 0.5);
    double nearFarRatio = 0.0005;

    // set up the camera
    auto perspective = vsg::Perspective::create(
        30.0,
        (double)(window->extent2D().width) / (double)(window->extent2D().height),
        nearFarRatio * bs.radius,
        bs.radius * 4.5);

    auto lookAt = vsg::LookAt::create(
        bs.center + vsg::dvec3(0.0, -bs.radius * 3.5, 0.0),
        bs.center,
        vsg::dvec3(0.0, 0.0, 1.0));

    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(
        window, 
        camera,
        vsg_scene,
        VK_SUBPASS_CONTENTS_INLINE,
        false); // assignHeadlight

    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

    viewer->compile();

    std::vector<std::chrono::microseconds> timeSamples;

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        auto start = clock::now();

        viewer->handleEvents();
        viewer->update();

        mapNode->update(viewer->getFrameStamp());

        viewer->recordAndSubmit();
        viewer->present();

        // sample the frame rate every N frames
        if ((viewer->getFrameStamp()->frameCount % 10) == 0) {
            auto end = std::chrono::steady_clock::now();
            auto t = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            timeSamples.push_back(t);
        }
    }

    std::chrono::microseconds total(0);
    for (auto sample : timeSamples)
        total += sample;
    total /= timeSamples.size();

    ROCKY_NOTICE << "Average frame time = " 
        << std::setprecision(3) << 0.001f * (float)total.count() << " ms" << std::endl;

    return 0;
}

