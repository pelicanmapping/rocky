/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <cstdlib> // rand
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Views = [](Application& app)
{
    // iterate over all managed windows:
    int window_id = 0;
    for (auto windows_iter : app.displayManager->windowsAndViews)
    {
        auto window = windows_iter.first;
        auto& views = windows_iter.second;

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
                    
                    auto rg = app.displayManager->getRenderGraph(view);

                    // the clear color, which resides in a renderpass attachment:
                    if (rg &&
                        rg->clearValues.size() > 0 &&
                        rg->getRenderPass()->attachments.size() > 0 &&
                        rg->getRenderPass()->attachments[0].format == VK_FORMAT_B8G8R8A8_UNORM)
                    {
                        auto& color = rg->clearValues[0].color.float32;
                        if (ImGuiLTable::ColorEdit3("Clear", color))
                        {
                            // just works - nothing to do
                        }
                    }

                    if (num > 1)  // dont' allow editing the first view
                    {
                        // the viewport - changing this requires a bunch of updates and a call to  app.refreshView
                        bool vp_dirty = false;
                        auto old_vp = view->camera->getViewport();
                        auto vp = view->camera->getViewport();
                        if (ImGuiLTable::SliderFloat("X", &vp.x, 0, window->traits()->width))
                        {
                            vp_dirty = true;
                        }
                        if (ImGuiLTable::SliderFloat("Y", &vp.y, 0, window->traits()->height))
                        {
                            vp_dirty = true;
                        }
                        if (ImGuiLTable::SliderFloat("Width", &vp.width, 0, window->traits()->width))
                        {
                            vp_dirty = true;
                        }
                        if (ImGuiLTable::SliderFloat("Height", &vp.height, 0, window->traits()->height))
                        {
                            vp_dirty = true;
                        }

                        if (vp_dirty)
                        {
                            if (vp.x + vp.width >= window->traits()->width) vp.x = window->traits()->width - vp.width - 1;
                            if (vp.y + vp.height >= window->traits()->height) vp.y = window->traits()->height - vp.height - 1;
                            view->camera->projectionMatrix->changeExtent(VkExtent2D{ (unsigned)old_vp.width, (unsigned)old_vp.height }, VkExtent2D{ (unsigned)vp.width, (unsigned)vp.height });
                            view->camera->viewportState->set(vp.x, vp.y, vp.width, vp.height);
                            app.displayManager->refreshView(view);
                        }

                        if (ImGui::Button("Remove view"))
                        {
                            auto dm = app.displayManager;
                            app.queue([=]() { dm->removeView(view); });
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
                if (ImGui::Button("Add view"))
                {
                    // First make a camera for the new view, placed at a random location.
                    const double nearFarRatio = 0.00001;
                    const double vfov = 30.0;
                    const int width = 320, height = 200;
                    double R = app.mapNode->mapSRS().ellipsoid().semiMajorAxis();
                    int win_width = window->extent2D().width, win_height = window->extent2D().height;
                    int x = std::max(0, (rand() % win_width) - width);
                    int y = std::max(0, (rand() % win_height) - height);
                    double ar = (double)width / (double)height;

                    auto camera = vsg::Camera::create(
                        vsg::Perspective::create(vfov, ar, R * nearFarRatio, R * 20.0),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));


                    // create the new view:
                    auto new_view = vsg::View::create(camera, app.root);

                    auto dm = app.displayManager;
                    auto add = [=]()
                        {
                            dm->addViewToWindow(new_view, window);
                            auto rg = dm->getRenderGraph(new_view);
                            auto& color = rg->clearValues[0].color.float32;
                            color[0] = float(rand() % 255) / 255.0f;
                            color[1] = float(rand() % 255) / 255.0f;
                            color[2] = float(rand() % 255) / 255.0f;
                        };
                    app.queue(add);
                }
                ImGui::Unindent();
            }
        }
        ImGui::PopID();
    }

    ImGui::Indent();
    {
        ImGui::Separator();
        if (ImGui::Button("Add window (DISABLED for NOW)"))
        {
#if 0
            auto dm = app.displayManager;
            app.queue([=]() {
                auto name = std::string("Window ") + std::to_string(window_id);
                dm->addWindow(vsg::WindowTraits::create(800, 600, name));
            });
#endif
        }
        ImGui::Unindent();
    }
};
