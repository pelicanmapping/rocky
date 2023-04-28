/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include "helpers.h"
using namespace ROCKY_NAMESPACE;


auto Demo_Mesh = [](Application& app)
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
                dvec3 v1, v2, v3, v4;
                xform(dvec3{ lon, lat, alt }, v1);
                xform(dvec3{ lon + step, lat, alt }, v2);
                xform(dvec3{ lon + step, lat + step, alt }, v3);
                xform(dvec3{ lon, lat + step, alt }, v4);

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

    if (ImGuiLTable::Begin("TriangleMesh"))
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
