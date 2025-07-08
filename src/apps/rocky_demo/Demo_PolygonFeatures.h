/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/FeatureView.h>
#include <rocky/GDALFeatureSource.h>
#include <random>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;


auto Demo_PolygonFeatures = [](Application& app)
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

            // create a feature view and add features to it
            if (data->fs->featureCount() > 0)
                feature_view.features.reserve(data->fs->featureCount());

            auto iter = data->fs->iterate(app.context->io);
            while (iter.hasMore())
            {
                auto feature = iter.next();
                if (feature.valid())
                {
                    feature_view.features.emplace_back(std::move(feature));
                }
            }

            // generate random colors for the feature geometry:
            std::uniform_real_distribution<float> frand(0.15f, 1.0f);

            feature_view.styles.mesh_function = [&frand](const Feature& f)
                {
                    std::default_random_engine re(f.id);
                    MeshStyle ms;
                    ms.color = vsg::vec4{ frand(re), frand(re), frand(re), 1.0f };
                    ms.depth_offset = 9000.0f; // highest point on earth
                    return ms;
                };

            // compile the features into renderable geometry
            auto prims = feature_view.generate(app.mapNode->worldSRS(), app.context);

            if (!prims.empty())
            {
                app.registry.write([&](entt::registry& registry)
                    {
                        entities.emplace_back(prims.moveToEntity(registry));
                    });
            }
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Polygon features"))
    {
        if (!entities.empty())
        {
            auto [lock, registry] = app.registry.read();

            bool visible = ecs::visible(registry, entities.front());
            if (ImGuiLTable::Checkbox("Show", &visible))
            {
                ecs::setVisible(registry, entities.begin(), entities.end(), visible);
            }

            ImGuiLTable::End();
        }
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
