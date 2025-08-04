/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/SkyNode.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Environment = [](Application& app)
{
    if (app.skyNode == nullptr)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Sky is not installed; use --sky");

#if 0
        if (ImGui::Button("Install sky"))
        {
            app.vsgcontext->onNextUpdate([&app]()
                {
                    app.skyNode = SkyNode::create(app.vsgcontext);
                    app.mainScene->children.insert(app.mainScene->children.begin(), app.skyNode);
                    app.vsgcontext->compile(app.skyNode);
                });
        }
#endif

        app.vsgcontext->requestFrame();

        return;
    }

    static DateTime dt;

    if (ImGuiLTable::Begin("environment"))
    {
        float hours = (float)dt.hours();
        if (ImGuiLTable::SliderFloat("Time of day (UTC)", &hours, 0.0f, 23.999f, "%.1f"))
        {
            dt = DateTime(dt.year(), dt.month(), dt.day(), hours);
            app.skyNode->setDateTime(dt);
        }

        float ambient = app.skyNode->ambient->color.r;
        if (ImGuiLTable::SliderFloat("Ambient level", &ambient, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            app.skyNode->ambient->color = { ambient, ambient, ambient };
        }

        static bool atmo = true;
        if (ImGuiLTable::Checkbox("Show atmosphere", &atmo))
        {
            app.skyNode->setShowAtmosphere(atmo);
        }

        ImGuiLTable::End();
    }
};
