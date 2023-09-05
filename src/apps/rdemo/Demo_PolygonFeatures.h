/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/FeatureView.h>
#include <random>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_PolygonFeatures = [](Application& app)
{
#ifdef GDAL_FOUND
    static entt::entity entity = entt::null;
    static Status status;
    
    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to open feature source");
        return;
    }

    if (entity == entt::null)
    {
        // open a feature source:
        auto fs = rocky::OGRFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        status = fs->open();
        if (status.failed())
            return;

        // create a feature view and add features to it:
        entity = app.entities.create();
        FeatureView& feature_view = app.entities.emplace<FeatureView>(entity);

        if (fs->featureCount() > 0)
            feature_view.features.reserve(fs->featureCount());

        auto iter = fs->iterate(app.instance.ioOptions());
        while (iter->hasMore())
        {
            auto feature = iter->next();
            if (feature.valid())
            {
                feature.interpolation = GeodeticInterpolation::RhumbLine;
                feature_view.features.emplace_back(std::move(feature));
            }
        }

        // generate random colors for the feature geometry:
        std::default_random_engine re(0);
        std::uniform_real_distribution<float> frand(0.15f, 1.0f);

        feature_view.styles.mesh_function = [&](const Feature& f)
        {
            return MeshStyle{
                { frand(re), frand(re), frand(re), 1.0f },
                64.0f };
        };

        // compile the features into renderable geometry
        feature_view.generate(app.entities, app.instance.runtime());
    }

    if (ImGuiLTable::Begin("Polygon features"))
    {
        auto& component = app.entities.get<FeatureView>(entity);
        ImGuiLTable::Checkbox("Visible", &component.active);

        ImGuiLTable::End();
    }

#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
