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

    static auto show = [&](entt::registry& r, bool toggle)
        {
            r.get<Visibility>(outline).visible.fill(toggle);
            r.get<Visibility>(placemark).visible.fill(toggle);
        };

    if (placemark == entt::null)
    {
        app.registry.write([&](entt::registry& reg)
            {
                // configure a line geometry that will display the outline of the selected place:
                outline = reg.create();

                auto& geom = reg.emplace<LineGeometry>(outline);

                auto& style = reg.emplace<LineStyle>(outline);
                style.color = Color::Yellow;
                style.depthOffset = 9000.0f; //meters

                reg.emplace<Line>(outline, geom, style);

                // configure a label for the selected place:
                placemark = reg.create();

                // label:
                auto& label = reg.emplace<Label>(placemark, "");

                // Transform to place the entity:
                auto& xform = reg.emplace<Transform>(placemark);

                // Mark it invisible to start.
                show(reg, false);
            });

        app.vsgcontext->requestFrame();
    }

    else
    {
        if (ImGuiLTable::Begin("geocoding"))
        {
            if (ImGuiLTable::InputText("Location:", input_buf, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                // hide the graphics:
                app.registry.read([&](entt::registry& reg) {
                        show(reg, false);
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
            ImGui::Text("%s", "Searching...");
        }

        else if (geocoding_task.available())
        {
            auto result = geocoding_task.value();
            if (result.ok())
            {
                int count = 0;
                ImGui::Text("%s", "Click on a result to center:");

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

                                auto view = app.display.views(app.display.mainWindow()).front();
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

                                    fgen.features = { myfeature };
                                    auto primitives = fgen.generate(app.mapNode->srs());

                                    app.registry.write([&](entt::registry& reg)
                                        {
                                            // awkward. do something about this.
                                            auto& geom = reg.get<LineGeometry>(outline);
                                            geom.topology = primitives.lineGeom.topology;
                                            geom.points = primitives.lineGeom.points;
                                            geom.srs = primitives.lineGeom.srs;
                                            geom.colors = primitives.lineGeom.colors;
                                            geom.dirty(reg);

                                            show(reg, true);

                                            // update the label and the transform:
                                            auto&& [xform, label] = reg.get<Transform, Label>(placemark);

                                            auto text = display_name;
                                            util::replaceInPlace(text, ", ", "\n");
                                            label.text = text;

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

                    app.registry.read([&](entt::registry& reg) {
                            show(reg, false);
                        });
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.5, 0.5, 1), "Geocoding failed! %s", result.error().message.c_str());
            }
        }
    }
};
