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
        std::shared_ptr<FeatureSource> fs;
    };
    static Future<LoadedFeatures> data;
    static std::vector<entt::entity> entities;

    if (entities.empty())
    {
        if (data.empty())
        {
            data = app.io().services().jobs.dispatch([](auto& cancelable)
                {
                    auto fs = GDALFeatureSource::create();
                    fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
                    auto status = fs->open();
                    return LoadedFeatures{ status, fs };
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
                    // convert anything we find to lines:
                    feature.geometry.convertToType(Geometry::Type::LineString);
                    features.emplace_back(std::move(feature));
                });

            // a style for geometry creation:
            LineStyle style;
            style.color = StockColor::Yellow;
            style.width = 2.0f;
            style.resolution = 10000.0f;
            style.depthOffset = 25000.0f;

            // a geometry to populate:
            LineGeometry workingGeom;

            // create our builder and populate the geometry:
            FeatureBuilder builder;
            builder.buildLineGeometry(features, style, workingGeom);

            // create an entity and components to house the objects:
            app.registry.write([&](entt::registry& reg)
                {
                    auto e = reg.create();
                    auto& lineStyle = reg.emplace<LineStyle>(e, style);
                    auto& lineGeom = reg.emplace<LineGeometry>(e, workingGeom);
                    reg.emplace<Line>(e, lineGeom, lineStyle);

                    entities.emplace_back(e);
                });

            app.vsgcontext->requestFrame();
        }
        else
        {
            ImGui::TextUnformatted("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entities.front()).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            setVisible(reg, entities.begin(), entities.end(), v);
        }

        auto& line = reg.get<Line>(entities.front());
        auto& style = reg.get<LineStyle>(line.style);
        if (ImGuiLTable::Checkbox("Depth test", &style.depthTest))
            style.dirty(reg);

        if (ImGuiLTable::SliderFloat("Depth offset", &style.depthOffset, 0.0f, 100000.0f))
        {
            style.dirty(reg);
        }

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "%s", "Unavailable - not built with GDAL");
#endif
};
