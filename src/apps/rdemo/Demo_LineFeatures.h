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
    static entt::entity entity = entt::null;
    
    struct LoadedFeatures {
        Status status;
        std::shared_ptr<rocky::OGRFeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;

    if (entity == entt::null)
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
            // create a feature view and add features to it:
            entity = app.entities.create();
            FeatureView& feature_view = app.entities.emplace<FeatureView>(entity);

            auto iter = data->fs->iterate(app.instance.io());
            while (iter->hasMore())
            {
                auto feature = iter->next();
                if (feature.valid())
                {
                    // convert anything we find to lines:
                    feature.geometry.convertToType(Geometry::Type::LineString);

                    // use rhumb-line interpolation for linear features:
                    feature.interpolation = GeodeticInterpolation::RhumbLine;

                    feature_view.features.emplace_back(std::move(feature));
                }
            }

            // apply a style for geometry creation:
            feature_view.styles.line = LineStyle{
                { 1,1,0.3f,1 }, // color
                2.0f,          // width
                0xffff,        // stipple pattern
                1,             // stipple factor
                100000.0f };   // resolution (geometric error)

            // generate our renderable geometry
            feature_view.generate(app.entities, app.runtime());
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        auto& component = app.entities.get<FeatureView>(entity);
        ImGuiLTable::Checkbox("Visible", &component.active);

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
