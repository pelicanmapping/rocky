/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_FeatureView = [](Application& app)
    {
        static entt::entity entity = entt::null;

        if (entity == entt::null)
        {
            // A Feature represents geometry and properties in a spatial context.
            Feature feature;

            feature.interpolation = GeodeticInterpolation::GreatCircle;
            feature.geometry.type = Geometry::Type::LineString;
            feature.srs = SRS::WGS84;
            feature.geometry.points = {
                { -0.1276, 51.5074, 0.0 }, // London
                { 72.8777, 19.0760, 0.0 }, // Mumbai
                { 153.0211, -27.4698, 0.0 } // Brisbane
            };

            // Helper utility to build renderable components from our Feature:
            FeatureView view;
            view.features.emplace_back(std::move(feature));
            view.styles.lineStyle.color = Color::Yellow;
            view.styles.lineStyle.stipplePattern = 0xF0F0; // dashed line
            view.styles.lineStyle.depthOffset = 50000;

            entity = view.generate(app.mapNode->srs(), app.registry);

            app.vsgcontext->requestFrame();
        }

        ImGui::TextWrapped("%s", "FeatureView is a helper utility for turning GIS feature data into geometry (lines and meshes).");
    };
