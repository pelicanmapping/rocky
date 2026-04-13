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
    auto mainView = app.display.window(0).view(0);
    auto skyNode = mainView.find<SkyNode>();

    if (skyNode == nullptr)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", "Sky is not installed; use --sky");

#if 0
        if (ImGui::Button("Install sky"))
        {
            app.vsgcontext->onNextUpdate([&app, mainView](VSGContext vsgcontext)
                {
                    if (auto light = detail::find<vsg::Light>(app.scene))
                        app.scene->children.erase(std::remove(app.scene->children.begin(), app.scene->children.end(), light), app.scene->children.end());
                    auto skyNode = SkyNode::create(vsgcontext);
                    mainView.vsgView->children.insert(mainView.vsgView->children.begin(), skyNode);
                    vsgcontext->viewer()->compile();
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
            skyNode->setDateTime(dt);
        }

        float ambient = skyNode->ambient->color.r;
        if (ImGuiLTable::SliderFloat("Ambient level", &ambient, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            skyNode->ambient->color = { ambient, ambient, ambient };
        }

        static bool atmo = true;
        if (ImGuiLTable::Checkbox("Show atmosphere", &atmo))
        {
            skyNode->setShowAtmosphere(atmo);
        }

        ImGuiLTable::End();
    }
};
