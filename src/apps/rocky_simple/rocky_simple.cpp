/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <rocky/rocky.h>
#include <vsg/all.h>

int simple(int argc, char** argv);
int custom_window(int argc, char** argv);
int custom_frame_loop(int argc, char** argv);

int main(int argc, char** argv)
{
    rocky::Log()->info("Options are: ");
    rocky::Log()->info("  --simple               (the default)");
    rocky::Log()->info("  --custom-window        (create our own window and camera)");
    rocky::Log()->info("  --custom-frame-loop    (manage the VSG viewer and frame loop ourselves)");

    std::string a(argv[1]);

    if (argc >= 2 && a == "--custom-window")
        return custom_window(argc, argv);

    if (argc >= 2 && a == "--custom-frame-loop")
        return custom_frame_loop(argc, argv);

    return simple(argc, argv);
}


int simple(int argc, char** argv)
{
    rocky::Log()->info("\nRunning simply");

    rocky::Application app(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    return app.run();
}


int custom_window(int argc, char** argv)
{
    rocky::Log()->info("\nRunning with a custom window");

    rocky::Application app(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    double nearFarRatio = 0.00001;
    double R = app.mapNode->srs().ellipsoid().semiMajorAxis();

    auto traits = vsg::WindowTraits::create(1920, 1080, "window");
    auto window = vsg::Window::create(traits);

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

    auto view = vsg::View::create(camera, app.mainScene);

    app.display.addWindow(window, view);

    app.viewer->addEventHandler(vsg::Trackball::create(camera));

    app.vsgcontext->renderContinuously = true;

    return app.run();
}

int custom_frame_loop(int argc, char** argv)
{
    rocky::Log()->info("\nRunning with a custom frame loop and no Application object");

    auto viewer = vsg::Viewer::create();

    auto vsgcontext = rocky::VSGContextFactory::create(viewer);

    auto mapNode = rocky::MapNode::create(vsgcontext);
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    mapNode->map->add(layer);

    double nearFarRatio = 0.00001;
    double R = mapNode->srs().ellipsoid().semiMajorAxis();

    auto traits = vsg::WindowTraits::create(1920, 1080, argv[0]);
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

    auto view = vsg::View::create(camera, mapNode);
    auto rendergraph = vsg::RenderGraph::create(window, view);
    auto commandgraph = vsg::CommandGraph::create(window, rendergraph);
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandgraph });

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, vsgcontext));

    viewer->compile();

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}
