/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <rocky/rocky.h>
#include <vsg/all.h>

int simple(int argc, char** argv);
int custom_window(int argc, char** argv);
int no_app(int argc, char** argv);

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string_view(argv[1]) == "--simple")
        return simple(argc, argv);

    if (argc >= 2 && std::string_view(argv[1]) == "--custom-window")
        return custom_window(argc, argv);

    if (argc >= 2 && std::string_view(argv[1]) == "--no-app")
        return no_app(argc, argv);

    rocky::Log()->info("Options: ");
    rocky::Log()->info("  --simple           (rocky::Application, fully automated)");
    rocky::Log()->info("  --custom-window    (rocky::Application, but create our own window, camera, and manipulator)");
    rocky::Log()->info("  --no-app           (Manage the VSG viewer and frame loop ourselves, no rocky::Application)");
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

    auto lookAt = vsg::LookAt::create(
        vsg::dvec3(R * 10.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 0.0),
        vsg::dvec3(0.0, 0.0, 1.0));
    
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
