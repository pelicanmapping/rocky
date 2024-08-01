/**
 * rocky c++
 * Copyright 2024-2034 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Application.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#endif

int main(int argc, char** argv)
{
    rocky::Application app(argc, argv);

    if (app.commandLineStatus.failed())
    {
        rocky::Log()->error(app.commandLineStatus.message);
        exit(-1);
    }

    auto osm = rocky::TMSImageLayer::create();
    osm->setProfile(rocky::Profile::SPHERICAL_MERCATOR);
    osm->uri = "https://[abc].tile.openstreetmap.org/{z}/{x}/{y}.png";
    app.mapNode->map->layers().add(osm);

    return app.run();
}
