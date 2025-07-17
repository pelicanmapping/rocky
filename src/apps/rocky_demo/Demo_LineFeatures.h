/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/GDALFeatureSource.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_LineFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL
    
    struct LoadedFeatures {
        Status status;
        std::shared_ptr<rocky::FeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static std::vector<entt::entity> entities;

    if (entities.empty())
    {
        if (data.empty())
        {
            data = jobs::dispatch([](auto& cancelable)
                {
                    auto fs = rocky::GDALFeatureSource::create();
                    fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
                    auto status = fs->open();
                    return LoadedFeatures{ status, fs };
                });
        }
        else if (data.working())
        {
            ImGui::Text("Loading features...");
        }
        else if (data.available() && data->status.ok())
        {
            FeatureView feature_view;

            // create a feature view and add features to it:
            data->fs->each(app.vsgcontext->io, [&](Feature&& feature)
                {
                    // convert anything we find to lines:
                    feature.geometry.convertToType(Geometry::Type::LineString);
                    feature_view.features.emplace_back(std::move(feature));
                });

            // apply a style for geometry creation:
            feature_view.styles.line = LineStyle
            {
                { 1, 1, 0.3f, 1 }, // color
                2.0f,              // width (pixels)
                0xffff,            // stipple pattern (bitmask)
                1,                 // stipple factor
                100000.0f,         // resolution (geometric error)
                5000.0f            // depth offset (meters)
            };

            auto prims = feature_view.generate(app.mapNode->worldSRS(), app.vsgcontext);

            if (!prims.empty())
            {
                app.registry.write([&](entt::registry& registry)
                    {
                        auto e = prims.moveToEntity(registry);
                        entities.emplace_back(e);
                    });
            }
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        auto [lock, registry] = app.registry.read();

        bool v = visible(registry, entities.front());
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            setVisible(registry, entities.begin(), entities.end(), v);
        }

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
