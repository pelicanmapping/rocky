/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <unordered_map>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;

auto Demo_LabelFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    static std::optional<Status> status;
    static unsigned count = 0;
    const bool use_widgets = true;

    struct Candidate {
        double pop = 0.0;
        GeoPoint centroid;
        double area = 0.0;
    };
    static std::unordered_map<std::string, Candidate> candidates;
    static std::unordered_set<entt::entity> labels;
    static bool active = true;
    static float label_size_percentage = 100.0f;
    const float starting_label_size = 16.0f;

    if (!status.has_value())
    {
        Log()->info("Loading features...");

        auto fs = rocky::GDALFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        status = fs->open();

        // collect all the features, discarding duplicates by keeping the largest one
        auto iter = fs->iterate(app.context->io);
        while (iter.hasMore())
        {
            auto feature = iter.next();
            if (feature.valid())
            {
                auto name = feature.field("name").stringValue();
                if (!name.empty())
                {
                    auto& candidate = candidates[name];
                    auto area = feature.extent.area();
                    if (area > candidate.area)
                    {
                        candidate.area = area;
                        candidate.pop = feature.field("pop").doubleValue();
                        candidate.centroid = feature.extent.centroid();
                    }
                }
            }
        }

        auto [lock, registry] = app.registry.write();

        // create an entity for each candidate
        for (auto& [name, candidate] : candidates)
        {
            auto entity = registry.create();

#ifdef ROCKY_HAS_IMGUI

            // attach a component to control decluttering:
            auto& declutter = registry.emplace<Declutter>(entity);
            declutter.priority = (float)candidate.pop;

            auto& widget = registry.emplace<Widget>(entity);

            widget.render = [name=std::string(name)](WidgetInstance& i)
                {
                    auto& dc = i.registry.get<Declutter>(i.entity);

                    i.begin();
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                    i.render([&]() { ImGui::Text(name.c_str()); });
                    ImGui::PopStyleVar(2);                    
                    i.end();

                    // update the decluttering record to reflect our widget's size
                    dc.rect = Rect(i.size.x, i.size.y);
                };

#else

            // attach a label:
            auto& label = registry.emplace<Label>(entity);
            label.text = name;
            label.style.font = app.context->defaultFont;
            label.style.pointSize = starting_label_size;
            label.style.outlineSize = 0.2f;

            // attach a component to control decluttering:
            auto& declutter = registry.emplace<Declutter>(entity);
            declutter.priority = (float)candidate.pop;
            // note, 0.75 is points-to-pixels
            auto width = 0.75f * 0.60f * label.style.pointSize * (float)label.text.size();
            double height = 0.75 * label.style.pointSize;
            declutter.rect = Rect(width, height);
#endif

            // attach a transform to place the label:
            auto& transform = registry.emplace<Transform>(entity);
            transform.position = candidate.centroid;

            labels.emplace(entity);
        }
    }

    else if (ImGuiLTable::Begin("Label features"))
    {
        if (status->ok())
        {
            if (ImGuiLTable::Checkbox("Show", &active))
            {
                auto [lock, registry] = app.registry.write();

                for (auto entity : labels)
                {                    
                    if (active)
                        registry.emplace_or_replace<ActiveState>(entity);
                    else
                        registry.remove<ActiveState>(entity);
                }
            }

            ImGuiLTable::Text("Features:", "%ld", candidates.size());
            if (ImGuiLTable::SliderFloat("Label size", &label_size_percentage, 25.0f, 150.0f, "%.0f%%"))
            {
                auto [lock, registry] = app.registry.read();

                for (auto entity : labels)
                {
                    float size = starting_label_size * label_size_percentage * 0.01f;

                    auto& label = registry.get<Label>(entity);
                    label.style.pointSize = size;
                    label.dirty();

                    auto& declutter = registry.get<Declutter>(entity);
                    declutter.rect = Rect(0.75f * 0.60f * size * (float)label.text.size(), 0.75f * size);
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
