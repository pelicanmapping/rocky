/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <unordered_map>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;

#if IMGUI_VERSION_NUM >= 19200 && defined(_WIN32)
#define USE_DYNAMIC_FONTS
#endif

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
    static entt::entity styleEntity = entt::null;
    static bool active = true;
    static float minFontSize = 12.0f, maxFontSize = 32.0f;

    if (!status.has_value())
    {
        Log()->info("Loading features...");

        auto fs = rocky::GDALFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        auto r = fs->open();
        if (r.failed())
        {
            status = r.error();
            Log()->error("Failed to open GDAL feature source: {}", status->error().message);
            return;
        }

        status = Status{}; // ok!

        // collect all the features, discarding duplicates by keeping the largest one
        auto iter = fs->iterate(app.vsgcontext->io);
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


        app.registry.write([&](entt::registry& reg)
            {
                styleEntity = reg.create();
                auto& style = reg.emplace<LabelStyle>(styleEntity);
                style.fontName = ROCKY_DEMO_DEFAULT_FONT;

                // create an entity for each candidate
                for (auto& [name, candidate] : candidates)
                {
                    auto entity = reg.create();

                    auto& label = reg.emplace<Label>(entity, name, style);

                    // attach a transform to place the label:
                    auto& transform = reg.emplace<Transform>(entity);
                    transform.position = candidate.centroid;

                    // attach a component to control decluttering:
                    auto& declutter = reg.emplace<Declutter>(entity);
                    declutter.priority = (float)candidate.pop;

                    labels.emplace(entity);
                }
            });

        app.vsgcontext->requestFrame();
    }

    else if (ImGuiLTable::Begin("Label features"))
    {
        if (status->ok())
        {
            if (ImGuiLTable::Checkbox("Show", &active))
            {
                app.registry.write([&](entt::registry& reg)
                    {
                        for (auto entity : labels)
                        {
                            if (active)
                                reg.emplace_or_replace<ActiveState>(entity);
                            else
                                reg.remove<ActiveState>(entity);
                        }
                    });
            }

            auto [_, reg] = app.registry.read();
            auto& style = reg.get<LabelStyle>(styleEntity);

#ifdef USE_DYNAMIC_FONTS
            if (ImGuiLTable::SliderFloat("Font size", &style.textSize, minFontSize, maxFontSize, "%.0f"))
            {
                app.vsgcontext->requestFrame();
            }
#endif

            if (ImGuiLTable::SliderFloat("Outline size", &style.outlineSize, 0, 5, "%.0f"))
            {
                app.vsgcontext->requestFrame();
            }

            if (ImGuiLTable::SliderFloat("Border size", &style.borderSize, 0, 2, "%.0f"))
            {
                app.vsgcontext->requestFrame();
            }

            ImGuiLTable::Text("Features:", "%ld", candidates.size());
        }
        else
        {
            ImGui::TextColored(ImGuiErrorColor, "Failed to load features: %s", status->error().message.c_str());
        }
        ImGuiLTable::End();

        ImGui::TextWrapped("%s", "Tip: Use the Decluttering panel to prevent overlapping labels!");
    }

#else
    ImGui::TextColored(ImGuiErrorColor, "%s", "Unavailable - not built with GDAL");
#endif
};
