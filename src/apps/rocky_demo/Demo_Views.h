/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <random>
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Views = [](Application& app)
{
    // iterate over all managed windows:
    int window_id = 0;
    for (auto window : app.viewer->windows())
    {
        auto views = app.display.views(window);

        ImGui::PushID(window_id++);
        if (ImGui::TreeNodeEx(window->traits()->windowTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // for each window, iterate over all managed views in that window:
            int num = 0;
            for (auto& view : views)
            {
                ImGui::PushID(view->viewID*100);
                if (ImGui::TreeNodeEx("view", ImGuiTreeNodeFlags_DefaultOpen, "View %d", num++))
                {
                    ImGuiLTable::Begin("view");
                    
                    auto rg = app.display.renderGraph(view);

                    // the clear color, which resides in a renderpass attachment:
                    if (rg &&
                        rg->clearValues.size() > 0 &&
                        rg->getRenderPass()->attachments.size() > 0 &&
                        rg->getRenderPass()->attachments[0].format == VK_FORMAT_B8G8R8A8_UNORM)
                    {
                        ImGuiLTable::ColorEdit3("Clear", rg->clearValues[0].color.float32);
                    }

                    if (num > 1)  // dont' allow editing the first view
                    {
                        // the viewport - changing this requires a bunch of updates and a call to  app.refreshView
                        bool vp_dirty = false;
                        auto old_vp = view->camera->getViewport();
                        auto vp = view->camera->getViewport();
                        
                        if (ImGuiLTable::SliderFloat("X", &vp.x, 0, (float)window->traits()->width))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Y", &vp.y, 0, (float)window->traits()->height))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Width", &vp.width, 0, (float)window->traits()->width))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Height", &vp.height, 0, (float)window->traits()->height))
                            vp_dirty = true;

                        if (vp_dirty)
                        {
                            if (vp.x + vp.width >= (float)window->traits()->width) vp.x = (float)window->traits()->width - (float)vp.width - 1;
                            if (vp.y + vp.height >= (float)window->traits()->height) vp.y = (float)window->traits()->height - (float)vp.height - 1;
                            view->camera->projectionMatrix->changeExtent(VkExtent2D{ (unsigned)old_vp.width, (unsigned)old_vp.height }, VkExtent2D{ (unsigned)vp.width, (unsigned)vp.height });
                            view->camera->viewportState->set(
                                (std::uint32_t)vp.x, (std::uint32_t)vp.y,
                                (std::uint32_t)vp.width, (std::uint32_t)vp.height);

                            app.display.refreshView(view);
                        }

                        if (ImGui::Button("Remove view"))
                        {
                            app.onNextUpdate([&app, view]() { app.display.removeView(view); });
                        }
                    }

                    ImGuiLTable::End();
                    ImGui::Separator();
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();

            ImGui::Indent();
            {
                if (ImGui::Button("Add an inset view"))
                {
                    static std::mt19937 rng;
                    std::uniform_int_distribution next_int;

                    // First make a camera for the new view, placed at a random location.
                    const double nearFarRatio = 0.00001;
                    const double vfov = 30.0;
                    const int width = 320, height = 200;
                    double R = app.mapNode->srs().ellipsoid().semiMajorAxis();
                    int win_width = window->extent2D().width, win_height = window->extent2D().height;
                    int x = std::max(0, (next_int(rng) % win_width) - width);
                    int y = std::max(0, (next_int(rng) % win_height) - height);
                    double ar = (double)width / (double)height;

                    auto camera = vsg::Camera::create(
                        vsg::Perspective::create(vfov, ar, R * nearFarRatio, R * 20.0),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));

                    // create the new view:
                    auto new_view = vsg::View::create(camera, app.root);

                    auto add = [&app, new_view, window]()
                        {
                            std::uniform_int_distribution next_int;
                            app.display.addViewToWindow(new_view, window, true);
                            auto rg = app.display.renderGraph(new_view);
                            auto& color = rg->clearValues[0].color.float32;
                            color[0] = float(next_int(rng) % 255) / 255.0f;
                            color[1] = float(next_int(rng) % 255) / 255.0f;
                            color[2] = float(next_int(rng) % 255) / 255.0f;
                            app.display.refreshView(new_view);
                        };

                    app.onNextUpdate(add);
                }

#if 0
                //TODO: multi-window not working
                if (window_id > 1 && ImGui::Button("Close this window (EXPERIMENTAL)"))
                {
                    app.onNextUpdate([&app, window]() { app.display.removeWindow(window); });
                }
#endif

                ImGui::Unindent();
            }
        }
        ImGui::PopID();
    }

#if 0
    //TODO multi-window not working
    ImGui::Indent();
    {
        ImGui::Separator();
        if (ImGui::Button("Add a window (EXPERIMENTAL)"))
        {
            app.onNextUpdate([&app, window_id]() {
                auto name = std::string("Window ") + std::to_string(window_id);
                app.display.addWindow(vsg::WindowTraits::create(800, 600, name));
            });
        }

        ImGui::Unindent();
    }
#endif
};
