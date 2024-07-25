#include <rocky/vsg/Application.h>
#include <rocky/vsg/Icon.h>

#ifdef ROCKY_HAS_TMS
#include <rocky/TMSImageLayer.h>
#endif

#include <rocky/Geocoder.h>
#include <rocky/Feature.h>

int main(int argc, char** argv)
{
    rocky::Application app(argc, argv);

    // add a layer.
    auto osm = rocky::TMSImageLayer::create();
    osm->setProfile(rocky::Profile::SPHERICAL_MERCATOR);
    osm->uri = "https://[abc].tile.openstreetmap.org/{z}/{x}/{y}.png";
    app.mapNode->map->layers().add(osm);

    // Add an entity. You can't see it, but you can attach things to it.
    auto entity = app.entities.create();

    // A transform will place the entity in the scene.
    auto& xform = app.entities.emplace<rocky::Transform>(entity);
    xform.setPosition(rocky::GeoPoint(rocky::SRS::WGS84, 0, 0));

    // Attach something visible to the entity.
    auto& io = app.instance.ioOptions();
    auto image = io.services.readImageFromURI("https://raw.githubusercontent.com/gwaldron/osgearth/master/data/airport.png", io);
    if (image.status.ok())
    {
        auto& icon = app.entities.emplace<rocky::Icon>(entity);
        icon.image = image.value;
        icon.style = rocky::IconStyle{ 75, 0.0f }; // pixel size, rotation(radians)
    }

#ifdef ROCKY_HAS_GEOCODER
    // Find a place using the geocoder and move our entity there.
    // We'll do this in the background since the query could take a while.
    jobs::dispatch([&xform, &app]()
        {
            rocky::Geocoder geocoder;
            auto results = geocoder.geocode("London Heathrow", app.instance.ioOptions());
            if (results.status.ok())
                xform.setPosition(results->at(0).extent.centroid());
        });
#endif

    return app.run();
}
