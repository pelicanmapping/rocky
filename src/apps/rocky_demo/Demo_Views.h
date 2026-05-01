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
    static entt::entity borderEntity = entt::null;

    // Create a widget that will draw a border around inset views.
    if (borderEntity == entt::null)
    {
        auto [_, reg] = app.registry.write();

        borderEntity = reg.create();
        auto& w = reg.emplace<Widget>(borderEntity);
        w.render = [&](WidgetInstance& i)
            {
                const float thickness = 3.0f; // pixels

                ImGui::SetCurrentContext(i.context);

                ImGui::GetForegroundDrawList(ImGui::GetMainViewport())->AddRect(
                    ImVec2{ (float)i.view.viewport.xmin, (float)i.view.viewport.ymin },
                    ImVec2{ (float)i.view.viewport.xmax, (float)i.view.viewport.ymax },
                    StockColor::White.as(Color::Format::ABGR),
                    0.0f, // rounding
                    0,    // ImDrawFlags
                    thickness);
            };

        // no border on view 0 by default:
        auto& vis = reg.get<Visibility>(borderEntity);
        vis.visible[0] = false;
    }

    // iterate over all managed windows:
    int window_id = 0;

    for(auto& window : app.display.windows())
    {
        auto traits = window.vsgWindow->traits();

        ImGui::PushID(window_id++);
        if (ImGui::TreeNodeEx(traits->windowTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // for each window, iterate over all managed views in that window:
            for(auto iter = window.views().rbegin(); iter != window.views().rend(); ++iter)
            {
                auto& view = *iter;
                if (!view) continue;

                auto camera = view.vsgView->camera;

                ImGui::PushID(view.viewID * 100);
                if (ImGui::TreeNodeEx("view", ImGuiTreeNodeFlags_DefaultOpen, "View %d", view.viewID))
                {
                    ImGuiLTable::Begin("view");

                    // the clear color, which resides in a renderpass attachment:
                    if (view.renderGraph->clearValues.size() > 0)
                    {
                        ImGuiLTable::ColorEdit3("Clear color", view.renderGraph->clearValues[0].color.float32);
                    }

                    app.registry.read([&](entt::registry& reg)
                        {
                            auto& visibility = reg.get<Visibility>(borderEntity);
                            ImGuiLTable::Checkbox("Border", &visibility.visible[view.viewID]);
                        });

                    if (auto mapNode = view.find<MapNode>())
                    {
                        // Rendering profile:
                        static std::vector<std::string> options = { "global-geodetic", "global-qsc", "spherical-mercator", "plate-carree" };
                        int index = detail::indexOf(options, mapNode->profile.wellKnownName());
                        if (index >= 0)
                        {
                            if (ImGuiLTable::BeginCombo("Rendering profile", options[index].c_str()))
                            {
                                for (int i = 0; i < options.size(); ++i)
                                {
                                    if (ImGui::RadioButton(options[i].c_str(), index == i))
                                    {
                                        // Set the new profile:
                                        mapNode->profile = Profile(options[i]);

                                        // Set the appropriate camera projection type:

                                        if (mapNode->profile.srs().isProjected()) {
                                            camera->projectionMatrix = vsg::Orthographic::create(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
                                        }
                                        else {
                                            double ar = (double)camera->getViewport().width / (double)camera->getViewport().height;
                                            camera->projectionMatrix = vsg::Perspective::create(45.0, ar, 1.0, 1000.0);
                                        }

                                        // Reset the view's manipulator if it has one:
                                        if (auto manip = MapManipulator::get(view.vsgView))
                                            manip->home();

                                        // Request a rendering update:
                                        app.vsgcontext->requestFrame();
                                    }
                                }
                                ImGuiLTable::EndCombo();
                            }
                            ImGuiLTable::Text("Camera:", "%s", (is_perspective_projection_matrix(camera->projectionMatrix->transform())) ? "Perspective" : "Orthographic");
                        }
                    }

                    ImGuiLTable::SliderDouble("Shadow distance", &view.vsgView->viewDependentState->maxShadowDistance, 0.0, 1000000.0, "%.0lf", ImGuiSliderFlags_Logarithmic);

                    if (view.viewID > 0)  // don't allow position/size editing the main view
                    {
                        // the viewport - changing this requires a bunch of updates and a call to  app.refreshView
                        bool vp_dirty = false;
                        auto old_vp = camera->getViewport();
                        auto vp = camera->getViewport();
                        
                        if (ImGuiLTable::SliderFloat("X", &vp.x, 0, (float)traits->width))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Y", &vp.y, 0, (float)traits->height))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Width", &vp.width, 0, (float)traits->width))
                            vp_dirty = true;

                        if (ImGuiLTable::SliderFloat("Height", &vp.height, 0, (float)traits->height))
                            vp_dirty = true;

                        if (vp_dirty)
                        {
                            if (vp.x + vp.width >= (float)traits->width) vp.x = (float)traits->width - (float)vp.width - 1;
                            if (vp.y + vp.height >= (float)traits->height) vp.y = (float)traits->height - (float)vp.height - 1;
                            camera->projectionMatrix->changeExtent(VkExtent2D{ (unsigned)old_vp.width, (unsigned)old_vp.height }, VkExtent2D{ (unsigned)vp.width, (unsigned)vp.height });
                            camera->viewportState->set((std::uint32_t)vp.x, (std::uint32_t)vp.y, (std::uint32_t)vp.width, (std::uint32_t)vp.height);

                            view.dirty();
                        }

                        if (ImGui::Button("Remove view"))
                        {
                            app.onNextUpdate([&app, &window, &view](...) {
                                window.removeView(view); });
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
                vsg::ref_ptr<vsg::View> vsgView;
                static std::mt19937 rng;
                std::uniform_int_distribution next_int;

                if (ImGui::Button("Add a mini-map inset"))
                {
                    // First make a camera for the new view
                    const int width = 480, height = 480;
                    int x = window.vsgWindow->extent2D().width - width - 10, y = 10;

                    auto camera = vsg::Camera::create(
                        vsg::Orthographic::create(-1, 1, -1, 1, -1e10, 1e10),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));

                    auto new_mapNode = MapNode::create(app.vsgcontext);
                    new_mapNode->map = app.mapNode->map;
                    new_mapNode->profile = Profile("spherical-mercator");

                    auto group = vsg::Group::create();
                    group->addChild(new_mapNode);
                    group->addChild(app.systemsNode);

                    // create the new view:
                    vsgView = vsg::View::create(camera, group);
                }

                if (ImGui::Button("Add an shared inset"))
                {
                    static std::mt19937 rng;
                    std::uniform_int_distribution next_int;

                    // First make a camera for the new view, placed at a random location.
                    const double nearFarRatio = 0.00001;
                    const double vfov = 30.0;
                    const int width = 320, height = 200;
                    double R = app.mapNode->srs().ellipsoid().semiMajorAxis();
                    int win_width = window.vsgWindow->extent2D().width, win_height = window.vsgWindow->extent2D().height;
                    int x = std::max(0, (next_int(rng) % win_width) - width);
                    int y = std::max(0, (next_int(rng) % win_height) - height);
                    double ar = (double)width / (double)height;

                    auto camera = vsg::Camera::create(
                        vsg::Perspective::create(vfov, ar, R * nearFarRatio, R * 20.0),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));

                    // create the new view:
                    vsgView = vsg::View::create(camera, app.scene);
                }

                if (vsgView)
                {
                    auto add = [&app, vsgView, &window]()
                        {
                            std::uniform_int_distribution next_int;
                            View& view = window.addView(vsgView);
                            auto rg = view.renderGraph;
                            auto& color = rg->clearValues[0].color.float32;
                            color[0] = float(next_int(rng) % 64) / 255.0f;
                            color[1] = float(next_int(rng) % 64) / 255.0f;
                            color[2] = float(next_int(rng) % 64) / 255.0f;
                            view.dirty();
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
