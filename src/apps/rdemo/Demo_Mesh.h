/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Mesh.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Mesh_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        // Make an entity to hold our mesh:
        entity = app.entities.create();

        // Attach a mesh component:
        auto& mesh = app.entities.emplace<Mesh>(entity);

        // Make some geometry in ECEF coordinates
        auto xform = SRS::WGS84.to(SRS::WGS84.geocentricSRS());
        const double step = 2.5;
        const double alt = 0.0; 
        for (double lon = 0.0; lon < 35.0; lon += step)
        {
            for(double lat = 15.0; lat < 35.0; lat += step)
            {
                vsg::dvec3 v1, v2, v3, v4;
                xform(vsg::dvec3{ lon, lat, alt }, v1);
                xform(vsg::dvec3{ lon + step, lat, alt }, v2);
                xform(vsg::dvec3{ lon + step, lat + step, alt }, v3);
                xform(vsg::dvec3{ lon, lat + step, alt }, v4);

                mesh.add({ {v1, v2, v3} });
                mesh.add({ {v1, v3, v4} });
            }
        }

        // Set a dynamic style that we can change at runtime.
        mesh.style = { { 1,0.4,0.1,0.75 }, 32.0f, 1e-7f };

        // Turn off depth buffer writes
        mesh.writeDepth = false;
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto& mesh = app.entities.get<Mesh>(entity);

        ImGuiLTable::Checkbox("Visible", &mesh.active);

        if (mesh.style.has_value())
        {
            auto& style = mesh.style.value();
            float* col = (float*)&style.color;
            if (ImGuiLTable::ColorEdit4("Color", col))
                mesh.dirty();

            if (ImGuiLTable::SliderFloat("Wireframe", &style.wireframe, 0.0f, 32.0f, "%.0f"))
                mesh.dirty();

            if (ImGuiLTable::SliderFloat("Depth offset", &style.depth_offset, 0.0f, 0.00001f, "%.7f"))
                mesh.dirty();
        }

        ImGuiLTable::End();
    }
};



auto Demo_Mesh_Relative = [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        // Create a new entity to host our mesh
        entity = app.entities.create();

        // Attach the new mesh:
        Mesh& mesh = app.entities.emplace<Mesh>(entity);
        mesh.name = "Relative Mesh";

        // Make some geometry that will be relative to a geolocation:
        const float s = 250000.0;
        vsg::vec3 verts[8] = {
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

        vsg::vec4 color{ 1, 0, 1, 0.85 };

        for (unsigned i = 0; i < 48; )
        {
            mesh.add({
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });

            if ((i % 6) == 0)
                color.r *= 0.8, color.b *= 0.8;
        }

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates.
        auto& xform = app.entities.emplace<Transform>(entity);
        xform.node->position = GeoPoint(SRS::WGS84, 24.0, 24.0, s * 3.0);
        xform.node->bound.radius = s * sqrt(2);
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto& mesh = app.entities.get<Mesh>(entity);

        ImGuiLTable::Checkbox("Visible", &mesh.active);

        auto* style = app.entities.try_get<MeshStyle>(entity);
        if (style)
        {
            if (ImGuiLTable::ColorEdit4("Color", (float*)&style->color))
                mesh.dirty();

            if (ImGuiLTable::SliderFloat("Wireframe", &style->wireframe, 0.0f, 32.0f, "%.0f"))
                mesh.dirty();
        }

        auto& xform = app.entities.get<Transform>(entity).node;

        if (ImGuiLTable::SliderDouble("Latitude", &xform->position.y, -85.0, 85.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &xform->position.x, -180.0, 180.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &xform->position.z, 0.0, 2500000.0, "%.1lf"))
            xform->dirty();

        ImGuiLTable::End();
    }
};


auto Demo_Mesh_Multi = [](Application& app)
{
    // Demonstrates adding multiple components of the same type to an entity.
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        entity = app.entities.create();
        Mesh& mesh = app.entities.emplace<Mesh>(entity);
        mesh.name = "Relative Mesh";

        const float s = 250000.0;
        vsg::vec3 verts[8] = {
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

        vsg::vec4 color{ 1, 0, 1, 0.85 };

        for (unsigned i = 0; i < 48; )
        {
            mesh.add({
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });

            if ((i % 6) == 0)
                color.r *= 0.8, color.b *= 0.8;
        }

        // Add a transform component so we can position our mesh relative
        // to some geospatial coordinates.
        auto& xform = app.entities.emplace<Transform>(entity);
        xform.setPosition(GeoPoint(SRS::WGS84, 24.0, 24.0, s * 3.0));
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        auto& mesh = app.entities.get<Mesh>(entity);

        ImGuiLTable::Checkbox("Visible", &mesh.active);

        auto* style = app.entities.try_get<MeshStyle>(entity);
        if (style)
        {
            float* col = (float*)&style->color;
            if (ImGuiLTable::ColorEdit4("Color", col))
            {
                mesh.dirty();
            }
            if (ImGuiLTable::SliderFloat("Wireframe", &style->wireframe, 0.0f, 32.0f, "%.0f"))
            {
                mesh.dirty();
            }
        }

        auto& transform = app.entities.get<Transform>(entity);
        auto& xform = transform.node;

        if (ImGuiLTable::SliderDouble("Latitude", &xform->position.y, -85.0, 85.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &xform->position.x, -180.0, 180.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &xform->position.z, 0.0, 2500000.0, "%.1lf"))
            xform->dirty();

        ImGuiLTable::End();
    }
};
