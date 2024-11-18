/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>
#include <rocky/vsg/FeatureView.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_LineFeatures = [](Application& app)
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
            auto[lock, registry] = app.registry.write();

            // create a feature view and add features to it:
            auto iter = data->fs->iterate(app.instance.io());
            while (iter.hasMore())
            {
                auto feature = iter.next();
                if (feature.valid())
                {
                    // convert anything we find to lines:
                    feature.geometry.convertToType(Geometry::Type::LineString);
                    feature_view.features.emplace_back(std::move(feature));
                }
            }

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

            // generate our renderable geometry
            feature_view.generate(registry, app.mapNode->worldSRS(), app.runtime());
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        auto [lock, registry] = app.registry.read();

        bool visible = ecs::visible(registry, feature_view.entity);
        if (ImGuiLTable::Checkbox("Show", &visible))
            ecs::setVisible(registry, feature_view.entity, visible);

        if (feature_view.styles.line.has_value())
        {
            float* col = (float*)&feature_view.styles.line->color;
            if (ImGuiLTable::ColorEdit3("Color", col))
                feature_view.dirtyStyles(registry);

            if (ImGuiLTable::SliderFloat("Depth offset", &feature_view.styles.line->depth_offset, 0.0f, 20000.0f, "%.0f"))
                feature_view.dirtyStyles(registry);
        }

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
