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
                auto iter = data->fs->iterate(app.instance.io());
                while (iter.hasMore())
                {
                    auto feature = iter.next();
                    if (feature.valid())
                    {
                        auto field = feature.field("name");
                        if (!field.stringValue.empty())
                        {
                            auto entity = app.registry.create();
                            auto& label = app.registry.emplace<Label>(entity);
                            label.text = field.stringValue;
                            label.style.font = app.runtime().defaultFont;
                            label.style.pointSize = 28.0f;
                            label.style.outlineSize = 0.25f;

                            auto& transform = app.registry.emplace<Transform>(entity);
                            transform.setPosition(feature.extent.centroid());

                            auto& declutter = app.registry.emplace<Declutter>(entity);
                            declutter.priority = (float)feature.field("pop").doubleValue;
                        }
                    }
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
