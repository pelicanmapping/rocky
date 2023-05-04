/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <cstdlib> // rand
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Views = [=](Application& app)
{
    int window_id = 0;
    for (auto windows_iter : app.displayConfiguration.windows)
    {
        auto window = windows_iter.first;
        auto& views = windows_iter.second;

        ImGui::PushID(window_id++);
        if (ImGui::TreeNodeEx(window->traits()->windowTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            int num = 0;
            for (auto& view : views)
            {
                ImGui::PushID(view->viewID);
                if (ImGui::TreeNodeEx("view", ImGuiTreeNodeFlags_DefaultOpen, "View %d", num++))
                {
                    ImGuiLTable::Begin("view");
                    auto rendergraph = app.renderGraph(view);
                    if (rendergraph)
                    {
                        auto& cv = rendergraph->clearValues[0];
                        float col[3] = { cv.color.float32[0], cv.color.float32[1], cv.color.float32[2] };
                        if (ImGuiLTable::ColorEdit3("Clear", &col[0]))
                        {
                            // just assume the first clear value is the color attachment :)
                            rendergraph->setClearValues({ { col[0], col[1], col[2], 1.0f } });
                        }
                    }
                    ImGuiLTable::End();
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();

            ImGui::Indent();
            {
                if (ImGui::Button("New view"))
                {
                    // first make a camera for the new view:
                    const double nearFarRatio = 0.00001;
                    const double hfov = 30.0;
                    const int width = 320, height = 200;
                    double R = app.mapNode->mapSRS().ellipsoid().semiMajorAxis();
                    int win_width = window->extent2D().width, win_height = window->extent2D().height;
                    int x = std::max(0, (rand() % win_width) - width);
                    int y = std::max(0, (rand() % win_height) - height);
                    double ar = (double)width / (double)height;

                    auto camera = vsg::Camera::create(
                        vsg::Perspective::create(hfov, ar, R * nearFarRatio, R * 20.0),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));

                    auto new_view = vsg::View::create(camera, app.root);
                    app.addView(window, new_view);
                }
                ImGui::Unindent();
            }
        }
        ImGui::PopID();
    }

    ImGui::Indent();
    {
        ImGui::Separator();
        if (ImGui::Button("New window"))
        {
            auto name = std::string("Window ") + std::to_string(window_id);
            app.addWindow(vsg::WindowTraits::create(800, 600, name));
        }
        ImGui::Unindent();
    }
};
