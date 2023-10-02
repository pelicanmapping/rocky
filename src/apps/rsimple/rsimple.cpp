/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky_vsg/Application.h>

#if !defined(ROCKY_SUPPORTS_TMS)
#error This example requires TMS support. Check CMake.
#endif

#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#include <rocky/contrib/EarthFileImporter.h>

template<class T>
int error(T layer)
{
    rocky::Log()->warn("Problem with layer \"" + layer->name() + "\" : " + layer->status().message);
    return -1;
}

int main(int argc, char** argv)
{
    // instantiate the application engine.
    rocky::Application app(argc, argv);

    if (app.mapNode->map->layers().empty())
    {
        // add an imagery layer to the map
        auto layer = rocky::TMSImageLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        app.mapNode->map->layers().add(layer);

        // check for error
        if (layer->status().failed())
            return error(layer);

        // add an elevation layer to the map
        auto elevation = rocky::TMSElevationLayer::create();
        elevation->uri = "https://readymap.org/readymap/tiles/1.0.0/116/";
        app.mapNode->map->layers().add(elevation);

        // check for error
        if (elevation->status().failed())
            return error(elevation);
    }

    // run until the user quits.
    return app.run();
}
