/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/LineString.h>
#include <rocky_vsg/FeatureView.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_PolygonFeatures= [](Application& app)
{
    static std::shared_ptr<MapObject> object;
    static std::shared_ptr<FeatureView> feature_view;
    static bool visible = true;
    static Status status;
    
    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to open feature source");
        return;
    }

    if (!object)
    {
        // open a feature source:
        auto fs = rocky::OGRFeatureSource::create();
        fs->uri = "https://readymap.org/readymap/filemanager/download/public/countries.geojson";
        status = fs->open();
        if (status.failed())
            return;

        // create a feature view and add features to it:
        feature_view = FeatureView::create();
        auto iter = fs->iterate(app.instance.ioOptions());
        while (iter->hasMore())
        {
            auto feature = iter->next();
            if (feature.valid())
            {
                feature.interpolation = GeodeticInterpolation::RhumbLine;
                feature_view->features.emplace_back(std::move(feature));
            }
        }

        feature_view->styles.mesh_function = [&](const Feature& f)
        {
            return MeshStyle{ {
                (float)(std::rand() % 192 + 63) / 255.0f,
                (float)(std::rand() % 192 + 63) / 255.0f,
                (float)(std::rand() % 192 + 63) / 255.0f, 1.0f },
                64.0f };
        };

        // Finally, create an object with our attachment.
        app.add(object = MapObject::create(feature_view));
        return;
    }

    if (ImGuiLTable::Begin("Polygon features"))
    {
        if (ImGuiLTable::Checkbox("Visible", &visible))
        {
            if (visible)
                app.add(object);
            else
                app.remove(object);
        }

        ImGuiLTable::End();
    }
};
