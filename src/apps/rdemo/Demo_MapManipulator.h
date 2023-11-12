/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Viewpoint.h>
#include <rocky/SRS.h>
#include <rocky/vsg/MapManipulator.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Viewpoints = [](Application& app)
{
    auto view = app.displayConfiguration.windows.begin()->second.front();
    if (view)
    {
        auto manip = view->getObject<MapManipulator>(MapManipulator::tag);
        if (manip)
        {
            Viewpoint vp;
            vp = manip->getViewpoint();

            if (vp.valid())
            {
                ImGui::SeparatorText("Current viewpoint");
                ImGuiLTable::Begin("Viewpoint");

                ImGuiLTable::Text("ECEF X:", "%.1lf", vp.position().x);
                ImGuiLTable::Text("ECEF Y:", "%.1lf", vp.position().y);
                ImGuiLTable::Text("ECEF Z:", "%.1lf", vp.position().z);

                GeoPoint LL;
                if (vp.position().transform(vp.position().srs.geoSRS(), LL))
                {
                    ImGuiLTable::Text("Longitude:", "%.3lf", LL.x);
                    ImGuiLTable::Text("Latitude:", "%.3lf", LL.y);
                }

                ImGuiLTable::Text("Heading:", "%.1lf", (double)vp.heading.value());
                ImGuiLTable::Text("Pitch:", "%.1lf", (double)vp.pitch.value());
                ImGuiLTable::Text("Range:", "%.1lf", (double)vp.range.value());
                ImGuiLTable::End();
            }

            ImGui::SeparatorText("Fly to");
            static float duration_s = 2.0f;

            Viewpoint vp1;
            vp1.name = "Washington";
            vp1.heading = 0.0;
            vp1.pitch = -45.0;
            vp1.range = 250000.0;
            vp1.point = GeoPoint{ SRS::WGS84, -77.0, 38.9, 0.0 };

            if (ImGui::Button(vp1.name->c_str()))
            {
                manip->setViewpoint(vp1, std::chrono::duration<float>(duration_s));
            }

            Viewpoint vp2;
            vp2.name = "Barcelona";
            vp2.heading = -56.0;
            vp2.pitch = -25.0;
            vp2.range = 125000.0;
            vp2.point = GeoPoint{ SRS::WGS84, 2.16, 41.384, 0.0 };

            ImGui::SameLine();
            if (ImGui::Button(vp2.name->c_str()))
            {
                manip->setViewpoint(vp2, std::chrono::duration<float>(duration_s));
            }

            Viewpoint vp3;
            vp3.name = "Perth";
            vp3.heading = 0.0;
            vp3.pitch = -67;
            vp3.range = 30000.0;
            vp3.point = GeoPoint{ SRS::WGS84, 115.8, -32, 0.0 };

            ImGui::SameLine();
            if (ImGui::Button(vp3.name->c_str()))
            {
                manip->setViewpoint(vp3, std::chrono::duration<float>(duration_s));
            }

            ImGui::SameLine();
            if (ImGui::Button("Home"))
            {
                manip->home();
            }

            ImGuiLTable::Begin("fly to settings");
            ImGuiLTable::SliderFloat("Duration (s)", &duration_s, 0.0f, 10.0f, "%.1f");
            ImGuiLTable::End();
        }
    }
};
