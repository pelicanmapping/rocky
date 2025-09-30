/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Mesh_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Make an entity to hold our mesh:
        entity = reg.create();

        // Attach a mesh component:
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Make some geometry.
        const double step = 2.5;
        const double alt = 0.0;
        const double min_lon = 0.0, max_lon = 35.0;
        const double min_lat = 15.0, max_lat = 35.0;

        // A transform from WGS84 to the world SRS:
        auto xform = SRS::WGS84.to(app.mapNode->srs());

        for (double lon = 0.0; lon < 35.0; lon += step)
        {
            for(double lat = 15.0; lat < 35.0; lat += step)
            {
                auto v1 = xform(glm::dvec3(lon, lat, alt));
                auto v2 = xform(glm::dvec3(lon + step, lat, alt));
                auto v3 = xform(glm::dvec3(lon + step, lat + step, alt));
                auto v4 = xform(glm::dvec3(lon, lat + step, alt));
                
                geom.triangles.emplace_back(Triangle{ {v1, v2, v3} });
                geom.triangles.emplace_back(Triangle{ {v1, v3, v4} });
            }
        }

        auto& style = reg.emplace<MeshStyle>(entity);

        // Set a dynamic style that we can change at runtime.
        style.color = Color{ 1, 0.4f, 0.1f, 0.5f };
        style.depthOffset = 10000.0f;

        auto& mesh = reg.emplace<Mesh>(entity, geom, style);

        // Turn off depth buffer writes
        mesh.writeDepth = false;

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto [_, reg] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(reg, entity, visible);

        auto& style = reg.get<MeshStyle>(entity);

        if (ImGuiLTable::ColorEdit4("Color", (float*)&style.color))
            style.dirty(reg);

        if (ImGuiLTable::SliderFloat("Depth offset", &style.depthOffset, 0.0f, 10000.0f, "%.0f"))
            style.dirty(reg);

        ImGuiLTable::End();
    }
};



auto Demo_Mesh_Relative = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Create a new entity to host our mesh
        entity = reg.create();

        // Attach the new mesh:
        auto& geom = reg.emplace<MeshGeometry>(entity);

        // Make some geometry that will be relative to a geolocation:
        const double s = 250000.0;
        glm::dvec3 verts[8] = {
            { -s, -s, -s },
            {  s, -s, -s },
            {  s,  s, -s },
            { -s,  s, -s },
            { -s, -s,  s },
            {  s, -s,  s },
            {  s,  s,  s },
            { -s,  s,  s }
        };
        unsigned indices[48] = {
            0,3,2, 0,2,1, 4,5,6, 4,6,7,
            1,2,6, 1,6,5, 3,0,4, 3,4,7,
            0,1,5, 0,5,4, 2,3,7, 2,7,6
        };

        Color color{ 1, 0, 1, 0.85f };

        for (unsigned i = 0; i < 48; )
        {
            geom.triangles.emplace_back(Triangle{
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });

            if ((i % 6) == 0)
                color.r *= 0.8f, color.b *= 0.8f;
        }

        auto& mesh = reg.emplace<Mesh>(entity, geom);

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates. We then set the bound on the node
        // to control horizon culling a little better
        auto& xform = reg.emplace<Transform>(entity);
        xform.topocentric = true;
        xform.position = GeoPoint(SRS::WGS84, 24.0, 24.0, s * 3.0);
        xform.radius = s * sqrt(2);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& transform = reg.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};
