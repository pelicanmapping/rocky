/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/ecs/Texture.h>

using namespace ROCKY_NAMESPACE;

//! Demonstrates relative and absolute polygons with holes and live controls for a shared PolygonStyle.
auto Demo_Polygon = [](Application& app)
{
    static entt::entity e_relative = entt::null;
    static entt::entity e_absolute = entt::null;
    static entt::entity e_style = entt::null;
    static entt::entity e_texture = entt::null;
    static Color textureColor(0.5f, 0.5f, 0.5f, 1.0f);

    if (e_relative == entt::null)
    {
        constexpr double centerLon = -122.4207;
        constexpr double centerLat = 37.7732;
        constexpr double altitude = 1800.0;

        app.registry.write([&](entt::registry& reg)
        {
            e_style = reg.create();
            auto& style = reg.emplace<PolygonStyle>(e_style);
            style.color = StockColor::Lime;

            // Polygon tessellation currently emits zero UVs. A solid texture makes
            // texture assignment and modulation testable for both coordinate systems.
            e_texture = reg.create();
            auto& texture = reg.emplace<ImageTexture>(e_texture);
            texture.image = Image::create(Image::R8G8B8A8_UNORM, 1, 1);
            texture.image->fill(textureColor);

            // Local coordinates are in meters, relative to a topocentric Transform.
            // The concave exterior winds counter-clockwise and the hole clockwise.
            e_relative = reg.create();
            auto& relativeGeometry = reg.emplace<PolygonGeometry>(e_relative);
            relativeGeometry.polygons.emplace_back(PolygonPart{
                {
                    { -2000.0, -2000.0, 0.0 },
                    {  2000.0, -2000.0, 0.0 },
                    {  1000.0,     0.0, 0.0 },
                    {  2000.0,  2000.0, 0.0 },
                    { -2000.0,  2000.0, 0.0 }
                },
                {{
                    { -1000.0, -700.0, 0.0 },
                    { -1000.0,  700.0, 0.0 },
                    {  -250.0,  700.0, 0.0 },
                    {  -250.0, -700.0, 0.0 }
                }}
            });
            relativeGeometry.colors.emplace_back(0.05f, 0.75f, 0.95f, 1.0f);
            reg.emplace<rocky::Polygon>(e_relative, relativeGeometry, style);

            auto& transform = reg.emplace<Transform>(e_relative);
            transform.position = GeoPoint(SRS::WGS84, centerLon - 0.035, centerLat, altitude);
            transform.topocentric = true;
            transform.radius = 3000.0;

            // Absolute longitude/latitude/altitude coordinates need no placement Transform.
            // Keep this polygon nearby and elevated so both are visible above the terrain.
            e_absolute = reg.create();
            auto& absoluteGeometry = reg.emplace<PolygonGeometry>(e_absolute);
            absoluteGeometry.srs = SRS::WGS84;
            absoluteGeometry.polygons.emplace_back(PolygonPart{
                {
                    { centerLon + 0.015, centerLat - 0.018, altitude },
                    { centerLon + 0.065, centerLat - 0.018, altitude },
                    { centerLon + 0.052, centerLat,         altitude },
                    { centerLon + 0.065, centerLat + 0.018, altitude },
                    { centerLon + 0.015, centerLat + 0.018, altitude }
                },
                {{
                    { centerLon + 0.027, centerLat - 0.006, altitude },
                    { centerLon + 0.027, centerLat + 0.006, altitude },
                    { centerLon + 0.037, centerLat + 0.006, altitude },
                    { centerLon + 0.037, centerLat - 0.006, altitude }
                }}
            });
            absoluteGeometry.colors.emplace_back(1.0f, 0.7f, 0.1f, 1.0f);
            reg.emplace<rocky::Polygon>(e_absolute, absoluteGeometry, style);
        });

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint viewpoint;
            viewpoint.point = GeoPoint(SRS::WGS84, centerLon, centerLat, altitude);
            viewpoint.range = 22000.0;
            viewpoint.pitch = -65.0;
            manip->setViewpoint(viewpoint, 1.0s);
        }

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped(
        "The west polygon uses local coordinates relative to a Transform. "
        "The east polygon uses WGS84 map coordinates. Both share the style below.");

    bool changed = false;
    app.registry.write([&](entt::registry& reg)
    {
        if (ImGuiLTable::Begin("polygon-component"))
        {
            //! Toggles an instance in every view while the registry write lock is held.
            auto visibilityControl = [&](const char* label, entt::entity entity)
            {
                auto& visibility = reg.get<Visibility>(entity);
                if (ImGuiLTable::Checkbox(label, &visibility.visible[0]))
                {
                    visibility.visible.fill(visibility.visible[0]);
                    changed = true;
                }
            };
            visibilityControl("Show relative polygon", e_relative);
            visibilityControl("Show map coordinates polygon", e_absolute);

            ImGuiLTable::SeparatorText("Shared style");
            auto& style = reg.get<PolygonStyle>(e_style);
            bool styleChanged = false;
            styleChanged |= ImGuiLTable::ColorEdit4("Color", reinterpret_cast<float*>(&style.color));
            styleChanged |= ImGuiLTable::Checkbox("Per-polygon colors", &style.useGeometryColors);
            styleChanged |= ImGuiLTable::Checkbox("Wireframe", &style.wireframe);
            styleChanged |= ImGuiLTable::SliderFloat(
                "Depth offset (m)", &style.depthOffset, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);

            int stipplePattern = static_cast<int>(style.stipplePattern & 0xFFFFu);
            if (ImGuiLTable::SliderInt("Stipple pattern", &stipplePattern, 0, 0xFFFF, "%04X",
                ImGuiSliderFlags_AlwaysClamp))
            {
                style.stipplePattern = static_cast<std::uint32_t>(stipplePattern);
                styleChanged = true;
            }

            bool useTexture = style.texture != entt::null;
            if (ImGuiLTable::Checkbox("Texture", &useTexture))
            {
                style.texture = useTexture ? e_texture : entt::null;
                styleChanged = true;
            }
            if (useTexture && ImGuiLTable::ColorEdit4("Texture color", reinterpret_cast<float*>(&textureColor)))
            {
                auto& texture = reg.get<ImageTexture>(e_texture);
                texture.image->fill(textureColor);
                texture.dirty(reg);
                changed = true;
            }

            // Zero selects the default curvature resolution. Limit custom values to
            // avoid accidentally creating millions of triangles while dragging the slider.
            bool defaultResolution = style.resolution == 0.0f;
            if (ImGuiLTable::Checkbox("Default resolution", &defaultResolution))
            {
                style.resolution = defaultResolution ? 0.0f : 1000.0f;
                styleChanged = true;
            }
            if (!defaultResolution)
            {
                styleChanged |= ImGuiLTable::SliderFloat(
                    "Resolution (m)", &style.resolution, 100.0f, 10000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            }

            if (styleChanged)
            {
                style.dirty(reg);
                changed = true;
            }

            if (style.useGeometryColors)
            {
                ImGuiLTable::SeparatorText("Geometry colors");
                auto& relativeGeometry = reg.get<PolygonGeometry>(e_relative);
                if (ImGuiLTable::ColorEdit4("Relative color", reinterpret_cast<float*>(&relativeGeometry.colors[0])))
                {
                    relativeGeometry.dirty(reg);
                    changed = true;
                }
                auto& absoluteGeometry = reg.get<PolygonGeometry>(e_absolute);
                if (ImGuiLTable::ColorEdit4("Map coordinates color", reinterpret_cast<float*>(&absoluteGeometry.colors[0])))
                {
                    absoluteGeometry.dirty(reg);
                    changed = true;
                }
            }

            ImGuiLTable::End();
        }
    });

    if (changed)
        app.vsgcontext->requestFrame();
};
