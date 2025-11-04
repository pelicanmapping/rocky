/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <random>

using namespace ROCKY_NAMESPACE;

auto Demo_Point = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        app.registry.write([&](entt::registry& r)
            {
                // Create a new entity to host our point set
                entity = r.create();

                // Build the geometry.
                auto& geometry = r.emplace<PointGeometry>(entity);

                // Let's transform geodetic (long, lat) points into our world SRS:
                auto xform = SRS::WGS84.to(app.mapNode->srs());

                geometry.points.reserve(360);
                geometry.colors.reserve(360);

                for (double lon = -180; lon < 180; lon += 1.0)
                {
                    auto world = xform(glm::dvec3(lon, 10.0 * sin(lon / 22.5), 120000.0));
                    geometry.points.emplace_back(world);

                    auto hsl = Color::White.asHSL();
                    hsl[0] = (float)((lon + 180.0) / 360.0); // hue
                    hsl[1] = 1.0f; // saturation
                    hsl[2] = 0.5f; // lightness
                    geometry.colors.emplace_back().fromHSL(hsl);
                }

                // Style our points.
                auto& style = r.emplace<PointStyle>(entity);
                style.color = Color::Cyan;
                style.width = 8.0f;
                style.antialias = 0.5f;

                // Point tied the geometry and the style together in an instance
                r.emplace<Point>(entity, geometry, style);

                app.vsgcontext->requestFrame();
            });
    }

    if (ImGuiLTable::Begin("absolute point set"))
    {
        auto [_, reg] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(reg, entity, visible);

        auto& style = reg.get<PointStyle>(entity);

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
            style.dirty(reg);

        bool perVertex = style.color.a == 0.0f;
        if (ImGuiLTable::Checkbox("Per-vertex colors", &perVertex))
        {
            style.color.a = perVertex ? 0.0f : 1.0f;
            style.dirty(reg);
        }

        if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.0f"))
            style.dirty(reg);

        if (ImGuiLTable::SliderFloat("Antialias", &style.antialias, 0.0f, 1.0f, "%.1f"))
            style.dirty(reg);

        ImGuiLTable::End();
    }
};
