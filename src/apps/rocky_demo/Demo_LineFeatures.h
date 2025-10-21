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
        Result<> status;
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
            ImGui::Text("%s", "Loading features...");
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
            feature_view.styles.line.color = Color::Yellow;
            feature_view.styles.line.width = 2.0f;
            feature_view.styles.line.resolution = 10000.0f;
            feature_view.styles.line.depthOffset = 5000.0f;

            auto prims = feature_view.generate(app.mapNode->srs());

            if (!prims.empty())
            {
                app.registry.write([&](entt::registry& registry)
                    {
                        auto e = prims.createEntity(registry);
                        entities.emplace_back(e);
                    });
            }

            app.vsgcontext->requestFrame();
        }
        else
        {
            ImGui::Text("%s", "Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        auto [lock, registry] = app.registry.read();

        auto& v = registry.get<Visibility>(entities.front()).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            setVisible(registry, entities.begin(), entities.end(), v);
        }

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "%s", "Unavailable - not built with GDAL");
#endif
};
