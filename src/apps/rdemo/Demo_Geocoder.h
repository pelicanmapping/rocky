/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>
#include <rocky/vsg/Label.h>
#include <rocky/vsg/Mesh.h>
#include <rocky/Geocoder.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Geocoder = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;
    static jobs::future<Result<std::vector<Feature>>> geocoding_task;
    static char input_buf[256];
    static LabelStyle label_style_point;
    static LabelStyle label_style_area;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Icon load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    if (entity == entt::null)
    {
        // Load an icon image
        auto& io = app.io();
        auto image = io.services.readImageFromURI("https://raw.githubusercontent.com/gwaldron/osgearth/master/data/placemark64.png", io);
        if (image.status.failed())
        {
            status = image.status;
            return;
        }

        // Make an entity to host our icon:
        entity = app.entities.create();

        // Attach the new Icon and set up its properties:
        auto& icon = app.entities.emplace<Icon>(entity);
        icon.image = image.value;
        icon.style = IconStyle{ 32, 0.0f }; // pixel size, rotation(radians)
        icon.active = false;

        // Attach a label:
        label_style_point.font = app.runtime().defaultFont;
        label_style_point.horizontalAlignment = vsg::StandardLayout::LEFT_ALIGNMENT;
        label_style_point.pointSize = 26.0f;
        label_style_point.outlineSize = 0.5f;
        label_style_point.pixelOffset = { icon.style.size_pixels, 0.0f, 0.0f };

        label_style_area.font = app.runtime().defaultFont;
        label_style_area.horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
        label_style_area.pointSize = 26.0f;
        label_style_area.outlineSize = 0.5f;

        auto& label = app.entities.emplace<Label>(entity);
        label.active = false;

        // Outline for location boundary:
        auto& feature_view = app.entities.emplace<FeatureView>(entity);
        feature_view.styles.line = LineStyle();
        feature_view.styles.line->color = vsg::vec4{ 1, 1, 0, 1 };
        feature_view.styles.line->depth_offset = 9000.0f; //meters
        feature_view.active = false;

        // Transform to place the entity:
        auto& xform = app.entities.emplace<Transform>(entity);
    }

    if (ImGuiLTable::Begin("geocoding"))
    {
        if (ImGuiLTable::InputText("Location:", input_buf, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            // hide the placemark:
            auto& icon = app.entities.get<Icon>(entity);
            icon.active = false;

            std::string input(input_buf);
            geocoding_task = jobs::dispatch([&app, input](jobs::cancelable& c)
                {
                    Result<std::vector<Feature>> result;
                    if (!c.canceled())
                    {
                        Geocoder geocoder;
                        result = geocoder.geocode(input, app.io());
                    }
                    return result;
                });
        }
        ImGuiLTable::End();
    }

    if (geocoding_task.working())
    {
        ImGui::Text("Searching...");
    }

    else if (geocoding_task.available())
    {
        auto& result = geocoding_task.value();
        if (result.status.ok())
        {
            ImGui::Text("Click on a result to center:");
            for (auto& feature : result.value)
            {
                bool selected = false;
                ImGui::Separator();
                std::string display_name = feature.field("display_name").stringValue;
                ImGui::Selectable(display_name.c_str(), &selected);
                if (selected)
                {
                    app.onNextUpdate([&, display_name]()
                        {
                            auto extent = feature.extent;
                            if (extent.area() == 0.0)
                                extent.expand(Distance(10, Units::KILOMETERS), Distance(10, Units::KILOMETERS));

                            auto view = app.displayManager->windowsAndViews.begin()->second.front();
                            auto manip = MapManipulator::get(view);
                            if (manip)
                            {
                                Viewpoint vp = manip->getViewpoint();
                                vp.point = extent.centroid();
                                manip->setViewpoint(vp, std::chrono::seconds(2));
                            }

                            auto& icon = app.entities.get<Icon>(entity);
                            FeatureView& feature_view = app.entities.get<FeatureView>(entity);
                            auto& label = app.entities.get<Label>(entity);

                            // show the placemark:
                            if (feature.geometry.type == Geometry::Type::Points)
                            {
                                icon.active = true;
                                feature_view.active = false;
                                label.style = label_style_point;
                            }
                            else
                            {
                                // update the mesh:
                                auto copy_of_feature = feature;
                                copy_of_feature.geometry.convertToType(Geometry::Type::LineString);
                                Geometry::iterator i(copy_of_feature.geometry);
                                while(i.hasMore()) for (auto& point : i.next().points) point.z = 500.0;
                                feature_view.clear(app.entities);
                                feature_view.features = { copy_of_feature };
                                feature_view.generate(app.entities, app.runtime());
                                feature_view.active = true;
                                icon.active = false;
                                label.style = label_style_area;
                            }

                            // update the label:
                            auto text = display_name;
                            replace_in_place(text, ", ", "\n");
                            label.text = text;
                            label.active = true;
                            label.dirty(); // to apply the new text.

                            // position it:
                            auto& xform = app.entities.get<Transform>(entity);
                            xform.setPosition(extent.centroid());
                        });
                }
            }
            ImGui::Separator();
            if (ImGui::Button("Clear"))
            {
                geocoding_task.reset();
                input_buf[0] = (char)0;

                app.onNextUpdate([&]()
                    {
                        auto& icon = app.entities.get<Icon>(entity);
                        icon.active = false;
                        auto& label = app.entities.get<Label>(entity);
                        label.active = false;
                        FeatureView& feature_view = app.entities.get<FeatureView>(entity);
                        feature_view.active = false;
                    });
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0.5, 0.5, 1), std::string("Geocoding failed! " + result.status.message).c_str());
        }
    }
};
