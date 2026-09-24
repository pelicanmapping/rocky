/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/FeatureBuilder.h>
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
            data = app.io().services().jobs.dispatch([](auto& cancelable)
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
            ImGui::TextUnformatted("Loading features...");
        }
        else if (data.available() && data->status.ok())
        {
            // read in our feature data:
            std::vector<Feature> features;
            if (data->fs->featureCount() > 0)
                features.reserve(data->fs->featureCount());

            data->fs->each(app.vsgcontext->io, [&](Feature&& feature)
                {
                    features.emplace_back(std::move(feature));
                });


            // a style for geometry creation:
            PolygonStyle style;
            style.depthOffset = 9000.0f;
            style.useGeometryColors = true;

            // a geometry to populate:
            PolygonGeometry workingGeom;

            // Build polygon rings and holes; PolygonSystem handles tessellation.
            FeatureBuilder builder;

            std::uniform_real_distribution<float> frand(0.15f, 1.0f);
            builder.colorFunction = [&frand](const Feature& f)
                {
                    std::default_random_engine re(f.id);
                    return Color{ frand(re), frand(re), frand(re), 1.0f };
                };

            builder.buildPolygonGeometry(features, style, workingGeom);

            // create an entity and components to house the objects:
            app.registry.write([&](entt::registry& reg)
                {
                    entity = reg.create();
                    auto& polygonStyle = reg.emplace<PolygonStyle>(entity, style);
                    auto& polygonGeom = reg.emplace<PolygonGeometry>(entity, std::move(workingGeom));
                    reg.emplace<rocky::Polygon>(entity, polygonGeom, polygonStyle);
                });

            ready = true;
            app.vsgcontext->requestFrame();
        }
        else
        {
            ImGui::TextUnformatted("Failed to load features!");
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

            auto& style = reg.get<PolygonStyle>(entity);
            if (ImGuiLTable::Checkbox("Wireframe", &style.wireframe))
            {
                style.dirty(reg);
                app.vsgcontext->requestFrame();
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
