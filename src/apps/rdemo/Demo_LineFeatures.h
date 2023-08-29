/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Line.h>
#include <rocky_vsg/FeatureView.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_LineFeatures= [](Application& app)
{
    static entt::entity entity = entt::null;

    if (entity == entt::null)
    {
        ImGui::Text("Wait...");

        // open a feature source:
        auto fs = rocky::OGRFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        auto fs_status = fs->open();
        ROCKY_HARD_ASSERT(fs_status.ok());

        // create a feature view and add features to it:
        entity = app.entities.create();
        FeatureView& feature_view = app.entities.emplace<FeatureView>(entity);

        auto iter = fs->iterate(app.instance.ioOptions());
        while (iter->hasMore())
        {
            auto feature = iter->next();
            if (feature.valid())
            {
                // convert anything we find to lines:
                feature.geometry.convertToType(Geometry::Type::LineString);

                feature.interpolation = GeodeticInterpolation::RhumbLine;
                feature_view.features.emplace_back(std::move(feature));
            }
        }

        // apply a style:
        feature_view.styles.line = LineStyle{
            { 1,1,0.3,1 }, // color
            2.0f,          // width
            0xffff,        // stipple pattern
            1,             // stipple factor
            100000.0f };     // resolution (geometric error)
            //0.0f };        // depth offset

        feature_view.generate(app.entities, app.instance.runtime());

        return;
    }

    if (ImGuiLTable::Begin("Line features"))
    {
        auto& component = app.entities.get<FeatureView>(entity);
        ImGuiLTable::Checkbox("Visible", &component.active);

        ImGuiLTable::End();
    }
};
