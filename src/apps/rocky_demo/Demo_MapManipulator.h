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

auto Demo_MapManipulator = [](Application& app)
{
    static bool spin = false;
    static float spinSpeed = 1.0f;

    auto first_view = app.display.viewAtWindowCoords(app.viewer->windows().front(), 0, 0);
    if (first_view)
    {
        auto manip = MapManipulator::get(first_view);
        if (manip)
        {
            Viewpoint vp = manip->viewpoint();

            if (vp.valid())
            {
                ImGui::SeparatorText("Focal point on map");
                ImGuiLTable::Begin("Viewpoint");

                ImGuiLTable::Text("SRS:", "%s", vp.position().srs.name());
                ImGuiLTable::Text("X:", "%.1lf", vp.position().x);
                ImGuiLTable::Text("Y:", "%.1lf", vp.position().y);
                ImGuiLTable::Text("Z:", "%.1lf", vp.position().z);

                GeoPoint LL = vp.position().transform(vp.position().srs.geodeticSRS());
                if (LL.valid())
                {
                    ImGuiLTable::Text("Longitude:", "%.3lf", LL.x);
                    ImGuiLTable::Text("Latitude:", "%.3lf", LL.y);
                }

                ImGuiLTable::Text("Heading:", "%.1lf", (double)vp.heading->value());
                ImGuiLTable::Text("Pitch:", "%.1lf", (double)vp.pitch->value());
                ImGuiLTable::Text("Range:", "%.1lf", (double)vp.range->value());

                ImGuiLTable::Checkbox("Lock azimuth", &manip->settings.lockAzimuthWhilePanning);
                ImGuiLTable::Checkbox("Zoom to mouse", &manip->settings.zoomToMouse);

                ImGuiLTable::End();
            }

            ImGui::SeparatorText("Automatic");
            ImGuiLTable::Begin("manip auto settings");
            ImGuiLTable::Checkbox("Spin", &spin);
            if (spin)
                ImGuiLTable::SliderFloat("Spin speed", &spinSpeed, 1.0f, 20.0f, "%.1f");
            ImGuiLTable::End();

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

            ImGuiLTable::SliderFloat("Duration (s)", &duration_s, 0.0f, 10.0f, "%.1f");

            if (spin)
            {
                auto tse = app.viewer->getFrameStamp()->time.time_since_epoch();
                auto now = 0.001 * (double)std::chrono::duration_cast<std::chrono::milliseconds>(tse).count();
                auto vp = manip->viewpoint();
                vp.heading = std::fmod(now * (double)spinSpeed, 360.0);
                manip->setViewpoint(vp);
            }
        }
    }
};
