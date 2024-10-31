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
    static bool initialized = false;
    
    struct LoadedFeatures {
        Status status;
        std::shared_ptr<rocky::OGRFeatureSource> fs;
    };
    static jobs::future<LoadedFeatures> data;
    static FeatureView feature_view;

    if (!initialized)
    {
        initialized = true;

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
            auto iter = data->fs->iterate(app.instance.io());
            while (iter.hasMore())
            {
                auto feature = iter.next();
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
            feature_view.generate(app.entities, app.mapNode->worldSRS(), app.runtime());
        }
        else
        {
            ImGui::Text("Failed to load features!");
        }
    }

    else if (ImGuiLTable::Begin("Line features"))
    {
        bool visible = feature_view.visible();
        if (ImGuiLTable::Checkbox("Visible", &visible))
            feature_view.setVisible(visible, app.entities);

        if (feature_view.styles.line.has_value())
        {
            float* col = (float*)&feature_view.styles.line->color;
            if (ImGuiLTable::ColorEdit3("Color", col))
                feature_view.dirtyStyles(app.entities);

            if (ImGuiLTable::SliderFloat("Depth offset", &feature_view.styles.line->depth_offset, 0.0f, 20000.0f, "%.0f"))
                feature_view.dirtyStyles(app.entities);
        }

        ImGuiLTable::End();
    }
#else
        ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
