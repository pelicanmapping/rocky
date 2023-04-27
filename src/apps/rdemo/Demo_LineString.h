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
    static shared_ptr<LineString> line;
    static bool visible = true;

    if (!line)
    {
        ImGui::Text("Wait...");

        line = LineString::create();

        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        for (double lon = -180.0; lon <= 0.0; lon += 2.5)
        {
            rocky::dvec3 ecef;
            if (xform(rocky::dvec3(lon, 0.0, 6500000), ecef))
                line->pushVertex(ecef);
        }

        line->setStyle(LineStyle{ { 1,1,0,1 }, 3.0f, 0xffff, 4 });

        object = MapObject::create(line);
        app.add(object);
        
        // by the next frame, the object will be alive in the scene
        return;
    }

    if (ImGuiLTable::Begin("LineString"))
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
