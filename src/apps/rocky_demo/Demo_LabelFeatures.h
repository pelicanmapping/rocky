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

#if IMGUI_VERSION_NUM >= 19200
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
    static bool active = true;
    static float fontSize = 20.0f;
    static float minFontSize = 13.0f;
    static float maxFontSize = 32.0f;
    static int outlineSize = 2;

    struct FontData {
        ImGuiContext* igc = nullptr;
        ImFont* font = nullptr;
    };
    static ViewLocal<FontData> fonts;

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


#if defined(_WIN32) && defined(USE_DYNAMIC_FONTS)
        // Load a new font. This will run once per frame in the WidgetSystem.
        app.getSystem<WidgetSystem>()->preRecord([&](std::uint32_t viewID, ImGuiContext* igc)
            {
                if (fonts[viewID].igc != igc)
                {
                    ImGui::SetCurrentContext(igc);
                    fonts[viewID].font = ImGui::GetIO().Fonts->AddFontFromFileTTF("c:/windows/fonts/arial.ttf");
                    fonts[viewID].igc = igc;
                    ImGui::GetIO().FontDefault = fonts[viewID].font;
                }
            });
#endif


        auto [lock, registry] = app.registry.write();

        static bool appliedFont = false;

        // create an entity for each candidate
        for (auto& [name, candidate] : candidates)
        {
            auto entity = registry.create();

#ifdef ROCKY_HAS_IMGUI

            // attach a component to control decluttering:
            auto& declutter = registry.emplace<Declutter>(entity);
            declutter.priority = (float)candidate.pop;

            auto& label = registry.emplace<Widget>(entity);

            label.render = [&, name=std::string(name)](WidgetInstance& i)
                {
                    auto& dc = i.registry.get<Declutter>(i.entity);

                    ImGui::SetCurrentContext(i.context);
                    ImGui::SetNextWindowBgAlpha(0.0f); // fully transparent background
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                    ImGui::SetNextWindowPos(ImVec2(i.position.x - i.size.x / 2, i.position.y - i.size.y / 2));
                    ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);

#ifdef USE_DYNAMIC_FONTS
                    ImGui::PushFont(nullptr, fontSize);
#endif
                    const ImVec4 outlineColor(0.05f, 0.05f, 0.05f, 1.0f);
                    ImGuiEx::TextOutlined(outlineColor, outlineSize, name);

#ifdef USE_DYNAMIC_FONTS
                    ImGui::PopFont();
#endif
                    i.size = ImGui::GetWindowSize();
                    ImGui::End();
                    ImGui::PopStyleVar(1);

                    // update the decluttering record to reflect our widget's size
                    dc.rect = Rect(i.size.x, i.size.y);
                };

#else

            // attach a label:
            auto& label = registry.emplace<Label>(entity);
            label.text = name;
            label.style.font = app.vsgcontext->defaultFont;
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

        app.vsgcontext->requestFrame();
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

#ifdef USE_DYNAMIC_FONTS
            if (ImGuiLTable::SliderFloat("Font size", &fontSize, minFontSize, maxFontSize, "%.0f"))
            {
                app.vsgcontext->requestFrame();
            }
#endif

            if (ImGuiLTable::SliderInt("Outline size", &outlineSize, 0, 5))
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

        ImGui::TextWrapped("%s", "Tip: You can declutter the labels in the Decluttering panel.");
    }

#else
    ImGui::TextColored(ImGuiErrorColor, "%s", "Unavailable - not built with GDAL");
#endif
};
