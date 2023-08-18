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
    static shared_ptr<MapObject> object;
    static shared_ptr<Mesh> mesh;
    static bool visible = true;

    if (!mesh)
    {
        ImGui::Text("Wait...");

        mesh = Mesh::create();

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

                mesh->add({ {v1, v2, v3} });
                mesh->add({ {v1, v3, v4} });
            }
        }

        // Set a dynamic style that we can change at runtime.
        mesh->style = MeshStyle{ { 1,0.4,0.1,0.75 }, 32.0f, 1e-7f };

        // Turn off depth buffer writes. (You can only do this when creating the mesh)
        mesh->writeDepth = false;

        object = MapObject::create(mesh);
        app.add(object);

        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        if (mesh->style.has_value())
        {
            MeshStyle& style = mesh->style.value();

            float* col = (float*)&style.color;
            if (ImGuiLTable::ColorEdit4("Color", col))
            {
                mesh->dirty();
            }

            if (ImGuiLTable::SliderFloat("Wireframe", &style.wireframe, 0.0f, 32.0f, "%.0f"))
            {
                mesh->dirty();
            }

            if (ImGuiLTable::SliderFloat("Depth offset", &style.depth_offset, 0.0f, 0.00001f, "%.7f"))
            {
                mesh->dirty();
            }
        }

        ImGuiLTable::End();
    }
};



auto Demo_Mesh_Relative = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<Mesh> mesh;
    static bool visible = true;

    if (!mesh)
    {
        ImGui::Text("Wait...");

        mesh = Mesh::create();

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
            mesh->add({
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });

            if ((i % 6) == 0)
                color.r *= 0.8, color.b *= 0.8;
        }

        //mesh->style = MeshStyle{ { 0.5, 0.0, 0.5, 1.0 }, 32.0f };

        mesh->underGeoTransform = true;

        object = MapObject::create(mesh);
        object->xform->setPosition(GeoPoint(SRS::WGS84, 24.0, 24.0, s * 3.0));
        app.add(object);

        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("Mesh"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        if (mesh->style.has_value())
        {
            MeshStyle& style = mesh->style.value();

            float* col = (float*)&style.color;
            if (ImGuiLTable::ColorEdit4("Color", col))
            {
                mesh->dirty();
            }
            if (ImGuiLTable::SliderFloat("Wireframe", &style.wireframe, 0.0f, 32.0f, "%.0f"))
            {
                mesh->dirty();
            }
        }

        auto& pos = object->xform->position();
        glm::fvec3 vec{ pos.x, pos.y, pos.z };

        if (ImGuiLTable::SliderFloat("Latitude", &vec.y, -85.0, 85.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, vec.x, vec.y, vec.z));
        }

        if (ImGuiLTable::SliderFloat("Longitude", &vec.x, -180.0, 180.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, vec.x, vec.y, vec.z));
        }

        if (ImGuiLTable::SliderFloat("Altitude", &vec.z, 0.0, 2500000.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs, vec.x, vec.y, vec.z));
        }

        ImGuiLTable::End();
    }
};
