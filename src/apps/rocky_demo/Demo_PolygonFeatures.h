/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/FeatureView.h>
#include <random>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;


auto Demo_PolygonFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    struct LoadedFeatures {
        Status status;
        std::shared_ptr<rocky::OGRFeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static FeatureView feature_view;

    if (feature_view.entity == entt::null)
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
        else if (data.available() && data->status.ok())
        {
            // create a feature view and add features to it
            if (data->fs->featureCount() > 0)
                feature_view.features.reserve(data->fs->featureCount());

            auto iter = data->fs->iterate(app.instance.io());
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
            feature_view.generate(app.entities, app.mapNode->worldSRS(), app.runtime());
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Polygon features"))
    {
        bool visible = ecs::visible(app.registry, feature_view.entity);
        if (ImGuiLTable::Checkbox("Visible", &visible))
            ecs::setVisible(app.registry, feature_view.entity, visible);

        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
