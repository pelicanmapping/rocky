/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Application.h>
#include <rocky/vsg/Icon.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#endif

int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

#ifdef ROCKY_HAS_TMS
    // add an imagery layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->layers().add(layer);

    // check for error
    if (layer->status().failed())
        return -1;
#endif

    // Load an image:
    auto& io = app.instance.ioOptions();
    auto image = io.services.readImageFromURI("https://user-images.githubusercontent.com/326618/236923465-c85eb0c2-4d31-41a7-8ef1-29d34696e3cb.png", io);

    // Create a new entity:
    auto entity = app.entities.create();

    // Add an icon to it:
    auto& icon = app.entities.emplace<rocky::Icon>(entity);
    icon.image = image.value;
    icon.style = rocky::IconStyle{ 75, 0.0f }; // size, rotation

    // Add a transform to it:
    auto& xform = app.entities.emplace<rocky::Transform>(entity);
    xform.setPosition(rocky::GeoPoint(rocky::SRS::WGS84, 0, 0, 50000));

    // run until the user quits.
    return app.run();
}
