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
        Status status;
        std::shared_ptr<rocky::FeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static std::vector<entt::entity> entities;
    static bool ready = false;
    static entt::entity primitivesEntity = entt::null;

    if (!ready)
    {
        if (data.empty())
        {
            data = jobs::dispatch([](auto& cancelable)
                {
                    auto fs = rocky::GDALFeatureSource::create();
                    fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
                    auto r = fs->open();
                    Status s;
                    if (r.failed()) s = r.error();
                    return LoadedFeatures{ s, fs };
                });
        }
        else if (data.working())
        {
            ImGui::Text("%s", "Loading features...");
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

            feature_view.styles.mesh.depthOffset = 9000.0f;
            feature_view.styles.mesh.useGeometryColors = true;

            feature_view.styles.meshColorFunction = [&frand](const Feature& f)
                {
                    std::default_random_engine re(f.id);
                    return Color{ frand(re), frand(re), frand(re), 1.0f };
                };

            // compile the features into renderable geometry
            auto prims = feature_view.generate(app.mapNode->srs());

            if (!prims.empty())
            {
                app.registry.write([&](entt::registry& registry)
                    {
                        primitivesEntity = prims.createEntity(registry);
                        entities.emplace_back(primitivesEntity);
                    });
            }

            ready = true;
            app.vsgcontext->requestFrame();
        }
        else
        {
            ImGui::Text("%s", "Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Polygon features"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entities.front()).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            setVisible(reg, entities.begin(), entities.end(), v);
        }

        static bool wireframe = false;
        if (ImGuiLTable::Checkbox("Wireframe", &wireframe))
        {
            auto& style = reg.get<MeshStyle>(primitivesEntity);
            style.wireframe = wireframe;
            style.dirty(reg);
        }

        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "%s", "Unavailable - not built with GDAL");
#endif
};
