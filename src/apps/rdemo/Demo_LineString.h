/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/LineString.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_LineString_Absolute = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<LineString> line;
    static bool visible = true;

    if (!line)
    {
        ImGui::Text("Wait...");

        line = LineString::create();
        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double alt = 125000;
        for (double lon = -180.0; lon <= 0.0; lon += 2.5)
        {
            glm::dvec3 ecef;
            if (xform(glm::dvec3(lon, -20.0, alt), ecef))
                line->pushVertex(ecef);
        }
        line->setStyle(LineStyle{ { 1,1,0,1 }, 3.0f, 0xffff, 4 });

        // Add an object with both attachments
        object = MapObject::create(line);
        app.add(object);

        return;
    }

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        LineStyle style = line->style();

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
        {
            style.color.a = 1.0f;
            line->setStyle(style);
        }

        if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.0f"))
        {
            line->setStyle(style);
        }

        if (ImGuiLTable::SliderInt("Stipple pattern", &style.stipple_pattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
        {
            line->setStyle(style);
        }

        if (ImGuiLTable::SliderInt("Stipple factor", &style.stipple_factor, 1, 4))
        {
            line->setStyle(style);
        }

        ImGuiLTable::End();
    }
};

auto Demo_LineString_Relative = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<LineString> line;
    static bool visible = true;

    if (!line)
    {
        ImGui::Text("Wait...");

        const double size = 500000;
        line = LineString::create();
        line->underGeoTransform = true;
        line->pushVertex(-size, -size, 0);
        line->pushVertex(size, -size, 0);
        line->pushVertex(0, size, 0);
        line->pushVertex(-size, -size, 0);
        line->setStyle(LineStyle{ {1,0,0,1}, 4.0f });
        
        // Add an object with both attachments
        object = MapObject::create(line);
        app.add(object);

        // Position the transform (will only apply to the relative attachment)
        object->xform->setPosition(GeoPoint(SRS::WGS84, 0.0, 0.0, 25000.0));
        
        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("relative linestring"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        LineStyle style = line->style();

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
        {
            style.color.a = 1.0f;
            line->setStyle(style);
        }

        GeoPoint pos = object->xform->position();
        glm::fvec3 v{ pos.x, pos.y, pos.z };

        if (ImGuiLTable::SliderFloat("Latitude", &v.y, -85.0, 85.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), v.x, v.y, v.z));
        }

        if (ImGuiLTable::SliderFloat("Longitude", &v.x, -180.0, 180.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), v.x, v.y, v.z));
        }
        
        if (ImGuiLTable::SliderFloat("Altitude", &v.z, 0.0, 2500000.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), v.x, v.y, v.z));
        }

        ImGuiLTable::End();
    }
};
