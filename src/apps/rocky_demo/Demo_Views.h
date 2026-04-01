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
                    if (rg && rg->clearValues.size() > 0)
                    {
                        ImGuiLTable::ColorEdit3("Clear color", rg->clearValues[0].color.float32);
                    }

                    app.registry.read([&](entt::registry& reg)
                        {
                            auto& visibility = reg.get<Visibility>(borderEntity);
                            ImGuiLTable::Checkbox("Border", &visibility.visible[view->viewID]);
                        });

                    auto mapNode = detail::find<MapNode>(view);
                    if (mapNode)
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
                                            view->camera->projectionMatrix = vsg::Orthographic::create(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
                                        }
                                        else {
                                            double ar = (double)view->camera->getViewport().width / (double)view->camera->getViewport().height;
                                            view->camera->projectionMatrix = vsg::Perspective::create(45.0, ar, 1.0, 1000.0);
                                        }

                                        // Reset the view's manipulator if it has one:
                                        if (auto manip = MapManipulator::get(view))
                                            manip->home();

                                        // Request a rendering update:
                                        app.vsgcontext->requestFrame();
                                    }
                                }
                                ImGuiLTable::EndCombo();
                            }
                            ImGuiLTable::Text("Camera:", "%s", (is_perspective_projection_matrix(view->camera->projectionMatrix->transform())) ? "Perspective" : "Orthographic");
                        }
                    }

                    if (num > 1)  // don't allow position/size editing the first view
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
                            app.onNextUpdate([&app, view](...) { app.display.removeView(view); });
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
                vsg::ref_ptr<vsg::View> newView;
                static std::mt19937 rng;
                std::uniform_int_distribution next_int;

                if (ImGui::Button("Add a mini-map inset"))
                {
                    // First make a camera for the new view, placed at a random location.
                    const int width = 480, height = 480;
                    int win_width = window->extent2D().width, win_height = window->extent2D().height;
                    int x = win_width - width - 10;
                    int y = 10;

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
                    newView = vsg::View::create(camera, group);
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
                    int win_width = window->extent2D().width, win_height = window->extent2D().height;
                    int x = std::max(0, (next_int(rng) % win_width) - width);
                    int y = std::max(0, (next_int(rng) % win_height) - height);
                    double ar = (double)width / (double)height;

                    auto mainView = app.display.views(app.display.mainWindow()).front();

                    auto camera = vsg::Camera::create(
                        vsg::Perspective::create(vfov, ar, R * nearFarRatio, R * 20.0),
                        vsg::LookAt::create(),
                        vsg::ViewportState::create(x, y, width, height));

                    // create the new view:
                    newView = vsg::View::create(camera, app.root);
                }

                if (newView)
                {
                    auto add = [&app, newView, window]()
                        {
                            std::uniform_int_distribution next_int;
                            app.display.addViewToWindow(newView, window, true);
                            auto rg = app.display.renderGraph(newView);
                            auto& color = rg->clearValues[0].color.float32;
                            color[0] = float(next_int(rng) % 64) / 255.0f;
                            color[1] = float(next_int(rng) % 64) / 255.0f;
                            color[2] = float(next_int(rng) % 64) / 255.0f;
                            app.display.refreshView(newView);
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
