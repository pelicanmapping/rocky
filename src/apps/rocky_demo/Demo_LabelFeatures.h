/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>
#include <rocky/vsg/Transform.h>
#include <rocky/vsg/Declutter.h>

#include "helpers.h"

using namespace ROCKY_NAMESPACE;


auto Demo_LabelFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    struct LoadedFeatures {
        Status status;
        std::shared_ptr<rocky::OGRFeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static bool ready = false;

    struct Candidate {
        double pop = 0.0;
        GeoPoint centroid;
        double area = 0.0;
    };

    if (!ready)
    {
        if (data.empty())
        {
            data = jobs::dispatch([](auto& cancelable)
                {
                    auto fs = rocky::OGRFeatureSource::create();
                    fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
                    auto status = fs->open();
                    return LoadedFeatures{ status, fs };
                });
        }
        else if (data.working())
        {
            ImGui::Text("Loading features...");
        }
        else if (data.available())
        {
            if (data->status.ok())
            {
                std::map<std::string, Candidate> candidates;

                auto iter = data->fs->iterate(app.instance.io());
                while (iter.hasMore())
                {
                    auto feature = iter.next();
                    if (feature.valid())
                    {
                        auto field = feature.field("name");
                        if (!field.stringValue.empty())
                        {
                            auto& candidate = candidates[field.stringValue];
                            auto area = feature.extent.area();
                            if (area > candidate.area)
                            {
                                candidate.area = area;
                                candidate.pop = feature.field("pop").doubleValue;
                                candidate.centroid = feature.extent.centroid();
                            }
                        }
                    }
                }

                for (auto& [name, candidate] : candidates)
                {
                    auto entity = app.registry.create();

                    // attach a label:
                    auto& label = app.registry.emplace<Label>(entity);
                    label.text = name;
                    label.style.font = app.runtime().defaultFont;
                    label.style.pointSize = 28.0f;
                    label.style.outlineSize = 0.5f;

                    // attach a transform to place the label:
                    auto& transform = app.registry.emplace<Transform>(entity);
                    transform.setPosition(candidate.centroid);

                    // attach a component to control decluttering:
                    auto& declutter = app.registry.emplace<Declutter>(entity);
                    declutter.priority = (float)candidate.pop;
                    // note, 0.75 is points-to-pixels
                    declutter.width_px = 0.75f * 0.60f * label.style.pointSize * (float)label.text.size();
                    declutter.height_px = 0.75f * label.style.pointSize;
                }

                ready = true;
            }
            else
            {
                ImGui::TextColored(ImVec4(1.f, .3f, .3f, 1.f), "Failed to load features: %s", data->status.message.c_str());
            }

        }
    }

    else if (ImGuiLTable::Begin("Label features"))
    {
        ImGui::TextWrapped("Tip: You can declutter the labels in the Simulation -> Decluttering panel.");
        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
