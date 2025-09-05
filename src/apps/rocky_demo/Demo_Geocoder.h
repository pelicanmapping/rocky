/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Geocoder.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Geocoder = [](Application& app)
{
    static jobs::future<Result<std::vector<Feature>>> geocoding_task;
    static char input_buf[256];
    static entt::entity placemark = entt::null;
    static entt::entity outline = entt::null;

    if (placemark == entt::null)
    {
        app.registry.write([&](entt::registry& registry)
            {
                outline = registry.create();
                registry.emplace<Line>(outline);

                // Make an entity to host our icon:
                placemark = registry.create();               

                // label:
                auto& widget = registry.emplace<Widget>(placemark);
                widget.text = "Nowhere";

                // Transform to place the entity:
                auto& xform = registry.emplace<Transform>(placemark);

                // Mark it inactive to start.
                registry.get<Visibility>(placemark).visible.fill(false);
            });

        app.vsgcontext->requestFrame();
    }

    else
    {
        if (ImGuiLTable::Begin("geocoding"))
        {
            if (ImGuiLTable::InputText("Location:", input_buf, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                // disable the placemark:
                app.registry.read([&](entt::registry& registry)
                    {
                        // hide the placemark:
                        registry.get<Visibility>(placemark).visible.fill(false);
                    });

                std::string input(input_buf);

                geocoding_task = jobs::dispatch([&app, input](jobs::cancelable& c) -> Result<std::vector<Feature>>
                    {
                        if (c.canceled())
                            return Failure_OperationCanceled;

                        Geocoder geocoder;
                        auto r = geocoder.geocode(input, app.io());
                        app.vsgcontext->requestFrame();
                        return r;
                    });

                app.vsgcontext->requestFrame();
            }
            ImGuiLTable::End();
        }

        if (geocoding_task.working())
        {
            ImGui::Text("Searching...");
        }

        else if (geocoding_task.available())
        {
            auto result = geocoding_task.value();
            if (result.ok())
            {
                int count = 0;
                ImGui::Text("Click on a result to center:");

                for (auto& feature : result.value())
                {
                    ImGui::PushID(count++);
                    bool selected = false;
                    ImGui::Separator();
                    auto display_name = feature.field("display_name").stringValue();
                    ImGui::Selectable(display_name.c_str(), &selected);
                    if (selected)
                    {
                        app.onNextUpdate([&app, myfeature(feature), display_name]() mutable
                            {
                                auto extent = myfeature.extent;
                                if (extent.area() == 0.0)
                                    extent.expand(Distance(10, Units::KILOMETERS), Distance(10, Units::KILOMETERS));

                                auto view = app.display.windowsAndViews.begin()->second.front();
                                auto manip = MapManipulator::get(view);
                                if (manip)
                                {
                                    Viewpoint vp = manip->viewpoint();
                                    vp.point = extent.centroid();
                                    vp.range = Distance(std::max(extent.width(Units::METERS) * 7.0, 2500.0), Units::METERS);
                                    manip->setViewpoint(vp, std::chrono::seconds(2));
                                }

                                // update the mesh:
                                myfeature.geometry.convertToType(Geometry::Type::LineString);

                                if (myfeature.geometry.type != Geometry::Type::Points)
                                {
                                    // Outline for location boundary:
                                    FeatureView fgen;
                                    fgen.styles.line.color = Color::Yellow;
                                    fgen.styles.line.depth_offset = 9000.0f; //meters

                                    fgen.features = { myfeature };
                                    auto primitives = fgen.generate(app.mapNode->srs());

                                    app.registry.write([&](entt::registry& registry)
                                        {
                                            registry.emplace_or_replace<Line>(outline, primitives.line);

                                            registry.get<Visibility>(outline).visible.fill(true);
                                            registry.get<Visibility>(placemark).visible.fill(true);

                                            // update the label and the transform:
                                            auto&& [xform, widget] = registry.get<Transform, Widget>(placemark);

                                            auto text = display_name;
                                            util::replaceInPlace(text, ", ", "\n");
                                            widget.text = text;

                                            xform.position = myfeature.extent.centroid();
                                            xform.dirty();
                                        });
                                }
                            });
                    }                                    
                    ImGui::PopID();
                }
                ImGui::Separator();
                if (ImGui::Button("Clear"))
                {
                    geocoding_task.reset();
                    input_buf[0] = (char)0;

                    app.registry.read([&](entt::registry& registry)
                        {
                            registry.get<Visibility>(outline).visible.fill(false);
                            registry.get<Visibility>(placemark).visible.fill(false);
                        });
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.5, 0.5, 1), std::string("Geocoding failed! " + result.error().message).c_str());
            }
        }
    }
};
