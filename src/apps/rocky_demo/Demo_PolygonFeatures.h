/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/FeatureView.h>
#include <rocky/GDALFeatureSource.h>
#include <rocky/ecs/EntityCollectionLayer.h>
#include <random>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;


auto Demo_PolygonFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    struct LoadedFeatures {
        Result<> status;
        std::shared_ptr<rocky::FeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static EntityCollectionLayer::Ptr layer;

    if (!layer || layer->entities.empty())
    {
        if (!layer)
        {
            layer = EntityCollectionLayer::create(app.registry);
            layer->name = "Demo_PolygonFeatures layer";
            app.mapNode->map->add(layer);
        }

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

            auto iter = data->fs->iterate(app.vsgcontext->io);
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
                    ms.color = Color{ frand(re), frand(re), frand(re), 1.0f };
                    ms.depth_offset = 9000.0f; // highest point on earth
                    return ms;
                };

            // compile the features into renderable geometry
            auto prims = feature_view.generate(app.mapNode->worldSRS());

            if (!prims.empty())
            {
                app.registry.write([&](entt::registry& registry)
                    {
                        layer->entities.emplace_back(prims.move(registry));
                    });
            }

            if (layer->open(app.io()).failed())
            {
                Log()->warn("Failed to open entity collection layer");
            }
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Polygon features"))
    {
        bool open = layer->isOpen();
        if (ImGuiLTable::Checkbox("Show", &open))
        {
            if (open) {
                auto r = layer->open(app.io());
                if (r.failed())
                    Log()->warn("Failed to open entity collection layer");
            }
            else {
                layer->close();
            }
            //setVisible(registry, entities.begin(), entities.end(), v);
        }
        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
