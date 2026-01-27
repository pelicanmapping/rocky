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
    static Future<LoadedFeatures> data;
    static bool ready = false;
    static entt::entity entity = entt::null;

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
            FeatureView featureView;

            // create a feature view and add features to it
            if (data->fs->featureCount() > 0)
                featureView.features.reserve(data->fs->featureCount());

            auto iter = data->fs->iterate(app.vsgcontext->io);
            while (iter.hasMore())
            {
                auto feature = iter.next();
                if (feature.valid())
                {
                    featureView.features.emplace_back(std::move(feature));
                }
            }

            // generate random colors for the feature geometry:
            std::uniform_real_distribution<float> frand(0.15f, 1.0f);

            featureView.styles.meshStyle.depthOffset = 9000.0f;
            featureView.styles.meshStyle.useGeometryColors = true;
            featureView.styles.meshColorFunction = [&frand](const Feature& f)
                {
                    std::default_random_engine re(f.id);
                    return Color{ frand(re), frand(re), frand(re), 1.0f };
                };

            entity = featureView.generate(app.mapNode->srs(), app.registry);

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

        if (entity != entt::null)
        {
            auto& v = reg.get<Visibility>(entity).visible[0];
            if (ImGuiLTable::Checkbox("Show", &v))
            {
                setVisible(reg, entity, v);
            }

            static bool wireframe = false;
            if (ImGuiLTable::Checkbox("Wireframe", &wireframe))
            {
                auto& style = reg.get<MeshStyle>(entity);
                style.wireframe = wireframe;
                style.dirty(reg);
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(1.f, .3f, .3f, 1.f), "%s", "No features loaded");
        }

        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "%s", "Unavailable - not built with GDAL");
#endif
};
