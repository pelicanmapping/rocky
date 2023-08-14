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

        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double step = 2.5;
        const double alt = 50000;
        for (double lon = 0.0; lon < 35.0; lon += step)
        {
            for(double lat = 15.0; lat < 35.0; lat += step)
            {
                glm::dvec3 v1, v2, v3, v4;
                xform(glm::dvec3{ lon, lat, alt }, v1);
                xform(glm::dvec3{ lon + step, lat, alt }, v2);
                xform(glm::dvec3{ lon + step, lat + step, alt }, v3);
                xform(glm::dvec3{ lon, lat + step, alt }, v4);

                mesh->addTriangle(v1, v2, v3);
                mesh->addTriangle(v1, v3, v4);
            }
        }

        mesh->setStyle(MeshStyle{ { 1,0.4,0.1,0.75 }, 32.0f });

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

        MeshStyle style = mesh->style();

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit4("Color", col))
        {
            mesh->setStyle(style);
        }

        if (ImGuiLTable::SliderFloat("Wireframe", &style.wireframe, 0.0f, 32.0f, "%.0f"))
        {
            mesh->setStyle(style);
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
        std::vector<vsg::vec3> v = {
            { -s, -s, -s },
            {  s, -s, -s },
            {  s,  s, -s },
            { -s,  s, -s },
            { -s, -s,  s },
            {  s, -s,  s },
            {  s,  s,  s },
            { -s,  s,  s }
        };

        mesh->addTriangle(v[0], v[3], v[2]);
        mesh->addTriangle(v[0], v[2], v[1]);
        mesh->addTriangle(v[4], v[5], v[6]);
        mesh->addTriangle(v[4], v[6], v[7]);
        mesh->addTriangle(v[1], v[2], v[6]);
        mesh->addTriangle(v[1], v[6], v[5]);
        mesh->addTriangle(v[3], v[0], v[4]);
        mesh->addTriangle(v[3], v[4], v[7]);
        mesh->addTriangle(v[0], v[1], v[5]);
        mesh->addTriangle(v[0], v[5], v[4]);
        mesh->addTriangle(v[2], v[3], v[7]);
        mesh->addTriangle(v[2], v[7], v[6]);

        mesh->setStyle(MeshStyle{ { 0.5, 0.0, 0.5, 1.0 }, 32.0f });

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

        MeshStyle style = mesh->style();

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit4("Color", col))
        {
            mesh->setStyle(style);
        }

        if (ImGuiLTable::SliderFloat("Wireframe", &style.wireframe, 0.0f, 32.0f, "%.0f"))
        {
            mesh->setStyle(style);
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
