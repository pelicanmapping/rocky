#include <rocky/Instance.h>
#include <rocky/Notify.h>
#include <rocky/GDALLayers.h>
#include <rocky_vsg/MapNode.h>
#include <vsg/all.h>

using namespace rocky;

int usage(const char* msg)
{
    std::cout << msg << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    Instance instance;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.read({ "--help", "-h", "-?" }))
    {
        return usage(argv[0]);
    }

    // options that get passed to VSG reader/writer modules?
    auto options = vsg::Options::create();
    arguments.read(options);

    // window configuration
    auto traits = vsg::WindowTraits::create();
    traits->windowTitle = "Rocky";
    traits->debugLayer = arguments.read({ "--debug" });
    traits->apiDumpLayer = arguments.read({ "--api" });
    auto window = vsg::Window::create(traits);

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // TODO: read this from an earth file
    auto mapNode = MapNode::create(instance);
    //auto layer = GDALImageLayer::create();
    //layer->setURI("D:/data/imagery/world.tif");
    //mapNode->getMap()->addLayer(layer);
    vsg_scene->addChild(mapNode);

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    double nearFarRatio = 0.0005;

    // set up the camera
    auto perspective = vsg::Perspective::create(
        30.0,
        static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
        nearFarRatio * radius,
        radius * 4.5);

    auto lookAt = vsg::LookAt::create(
        centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
        centre,
        vsg::dvec3(0.0, 0.0, 1.0));

    auto camera = vsg::Camera::create(
        perspective,
        lookAt,
        vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(
        window, 
        camera,
        vsg_scene,
        VK_SUBPASS_CONTENTS_INLINE,
        false);

    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        //TODO: MapNode updates here.
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}

