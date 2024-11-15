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
#include <unordered_map>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;


auto Demo_LabelFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    static std::optional<Status> status;
    static unsigned count = 0;

    struct Candidate {
        double pop = 0.0;
        GeoPoint centroid;
        double area = 0.0;
    };
    static std::unordered_map<std::string, Candidate> candidates;
    static std::unordered_set<entt::entity> labels;
    static bool active = true;
    static float label_size_percentage = 100.0f;
    const float starting_label_size = 26.0f;

    if (!status.has_value())
    {
        Log()->info("Loading features...");

        auto fs = rocky::OGRFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        status = fs->open();

        // collect all the features, discarding duplicates by keeping the largest one
        auto iter = fs->iterate(app.instance.io());
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

        // create an entity for each candidate
        for (auto& [name, candidate] : candidates)
        {
            auto entity = app.registry.create();

            // attach a label:
            auto& label = app.registry.emplace<Label>(entity);
            label.text = name;
            label.style.font = app.runtime().defaultFont;
            label.style.pointSize = starting_label_size;
            label.style.outlineSize = 0.35f;

            // attach a transform to place the label:
            auto& transform = app.registry.emplace<Transform>(entity);
            transform.setPosition(candidate.centroid);

            // attach a component to control decluttering:
            auto& declutter = app.registry.emplace<Declutter>(entity);
            declutter.priority = (float)candidate.pop;
            // note, 0.75 is points-to-pixels
            declutter.width_px = 0.75f * 0.60f * label.style.pointSize * (float)label.text.size();
            declutter.height_px = 0.75f * label.style.pointSize;

            labels.emplace(entity);
        }
    }

    else if (ImGuiLTable::Begin("Label features"))
    {
        if (status->ok())
        {
            if (ImGuiLTable::Checkbox("Show", &active))
            {
                for (auto entity : labels)
                {
                    app.registry.get<Visibility>(entity).active = active;
                }
            }

            ImGuiLTable::Text("Features:", "%ld", candidates.size());
            if (ImGuiLTable::SliderFloat("Label size", &label_size_percentage, 25.0f, 150.0f, "%.0f%%"))
            {
                for (auto entity : labels)
                {
                    auto& label = app.registry.get<Label>(entity);
                    auto& declutter = app.registry.get<Declutter>(entity);

                    float size = starting_label_size * label_size_percentage * 0.01f;
                    declutter.width_px = 0.75f * 0.60f * size * (float)label.text.size();
                    declutter.height_px = 0.75f * size;

                    label.style.pointSize = size;
                    label.revision++;
                }
            }
        }
        else
        {
            ImGui::TextColored(ImGuiErrorColor, "Failed to load features: %s", status->message.c_str());
        }
        ImGuiLTable::End();

        ImGui::TextWrapped("Tip: You can declutter the labels in the Decluttering panel.");
    }

#else
    ImGui::TextColored(ImGuiErrorColor, "Unavailable - not built with GDAL");
#endif
};
