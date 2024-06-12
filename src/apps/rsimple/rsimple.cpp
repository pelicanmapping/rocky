#include <rocky/vsg/Application.h>
#include <rocky/vsg/Mesh.h>

#ifdef ROCKY_HAS_MBTILES
#include <rocky/MBTilesImageLayer.h>
#endif

#include <iostream>

int main(int argc, char** argv)
{
    rocky::Application app;

#if 0
    auto layer = rocky::MBTilesImageLayer::create();
    layer->setURI("OAM-World-1-8-min-J80.mbtiles");
    app.mapNode->map->layers().add(layer);

    if (layer->status().failed())
    {
        std::cerr << layer->status().toString() << std::endl;
    }
#endif

    // World
    auto entity = app.entities.create();

    auto& xform = app.entities.emplace<rocky::Transform>(entity);
    xform.setPosition(rocky::GeoPoint(rocky::SRS::WGS84, 0, 0, 50000));

    // Mesh
    auto mesh_entity = app.entities.create();
    auto& mesh = app.entities.emplace<rocky::Mesh>(mesh_entity);

    auto mesh_xform = rocky::SRS::WGS84.to(rocky::SRS::WGS84.geocentricSRS());
    const double step = 2.5;
    const double alt_mesh = 0.0;
    for (double lon = 0.0; lon < 35.0; lon += step)
    {
        for (double lat = 15.0; lat < 35.0; lat += step)
        {
            vsg::dvec3 v1, v2, v3, v4;
            mesh_xform(vsg::dvec3{ lon, lat, alt_mesh }, v1);
            mesh_xform(vsg::dvec3{ lon + step, lat, alt_mesh }, v2);
            mesh_xform(vsg::dvec3{ lon + step, lat + step, alt_mesh }, v3);
            mesh_xform(vsg::dvec3{ lon, lat + step, alt_mesh }, v4);

            mesh.add({ {v1, v2, v3} });
            mesh.add({ {v1, v3, v4} });
        }
    }

    mesh.style = { {1, 0.4, 0.1, 0.75}, 32.0f, 1e-7f };
    mesh.writeDepth = true;

    return app.run();
}