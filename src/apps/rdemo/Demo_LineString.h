/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_LineString = [](Application& app)
{
    static shared_ptr<MapObject> object;
    static shared_ptr<LineString> absolute, relative;
    static bool visible = true;

    if (!absolute)
    {
        ImGui::Text("Wait...");

        absolute = LineString::create();
        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double alt = 125000;
        for (double lon = -180.0; lon <= 0.0; lon += 2.5)
        {
            rocky::dvec3 ecef;
            if (xform(rocky::dvec3(lon, -20.0, alt), ecef))
                absolute->pushVertex(ecef);
        }
        absolute->setStyle(LineStyle{ { 1,1,0,1 }, 3.0f, 0xffff, 4 });


        // LineString relative to the GeoTransform:
        const double size = 400000;
        relative = LineString::create();
        relative->referenceFrame = LineString::ReferenceFrame::Relative;
        relative->pushVertex(-size, -size, 0);
        relative->pushVertex(size, -size, 0);
        relative->pushVertex(0, size, 0);
        relative->pushVertex(-size, -size, 0);
        relative->setStyle(LineStyle{ {1,0,0,1}, 4.0f });
        
        // Add an object with both attachments
        object = MapObject::create(Attachments{ absolute, relative });
        app.add(object);

        // Position the transform (will only apply to the relative attachment)
        object->xform->setPosition(GeoPoint(SRS::WGS84, 0.0, 0.0, 25000.0));
        
        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGui::Checkbox("Visible", &visible))
    {
        if (visible)
            app.add(object);
        else
            app.remove(object);
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Absolute position (rhumb line)");

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        LineStyle style = absolute->style();

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
        {
            style.color.a = 1.0f;
            absolute->setStyle(style);
        }

        if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.0f"))
        {
            absolute->setStyle(style);
        }

        if (ImGuiLTable::SliderInt("Stipple pattern", &style.stipple_pattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
        {
            absolute->setStyle(style);
        }

        if (ImGuiLTable::SliderInt("Stipple factor", &style.stipple_factor, 1, 4))
        {
            absolute->setStyle(style);
        }

        ImGuiLTable::End();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Relative position (triangle)");

    if (ImGuiLTable::Begin("relative linestring"))
    {
        auto& pos = object->xform->position();
        fvec3 vec = pos.to_dvec3();

        if (ImGuiLTable::SliderFloat("Latitude", &vec.y, -85.0, 85.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), vec));
        }

        if (ImGuiLTable::SliderFloat("Longitude", &vec.x, -180.0, 180.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), vec));
        }
        
        if (ImGuiLTable::SliderFloat("Altitude", &vec.z, 0.0, 1000000.0, "%.1f"))
        {
            object->xform->setPosition(GeoPoint(pos.srs(), vec));
        }

        ImGuiLTable::End();
    }
};
