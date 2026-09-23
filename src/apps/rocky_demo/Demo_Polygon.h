/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

// Demonstrates one shared PolygonGeometry rendered normally and through each
// available overlay pathway. The geometry is local to each topocentric
// Transform, contains a hole, and carries one color per polygon part.
auto Demo_Polygon = [](Application& app)
{
    static entt::entity e_geometry = entt::null;
    static entt::entity e_autofit_geometry = entt::null;
    static entt::entity e_style = entt::null;
    static entt::entity e_normal = entt::null;
    static entt::entity e_rtt = entt::null;
    static entt::entity e_autofit_rtt = entt::null;
    static entt::entity e_vector = entt::null;

    if (e_normal == entt::null)
    {
        constexpr double centerLon = -122.4207;
        constexpr double centerLat = 37.7732;

        app.registry.write([&](entt::registry& reg)
        {
            e_geometry = reg.create();
            auto& geometry = reg.emplace<PolygonGeometry>(e_geometry);

            // A concave polygon with a hole. Rings are implicitly closed.
            geometry.polygons.emplace_back(PolygonPart{
                {
                    { -0.500, -0.500, 0.0 },
                    { -0.167, -0.500, 0.0 },
                    { -0.083,  0.000, 0.0 },
                    { -0.167,  0.500, 0.0 },
                    { -0.500,  0.500, 0.0 },
                    { -0.389,  0.000, 0.0 }
                },
                {{
                    { -0.306, -0.125, 0.0 },
                    { -0.306,  0.125, 0.0 },
                    { -0.181,  0.125, 0.0 },
                    { -0.181, -0.125, 0.0 }
                }}
            });

            // A second independent polygon makes this a multipolygon asset.
            geometry.polygons.emplace_back(PolygonPart{
                {
                    { 0.069, -0.500, 0.0 },
                    { 0.500, -0.500, 0.0 },
                    { 0.389,  0.500, 0.0 },
                    { 0.167,  0.500, 0.0 }
                },
                {}
            });

            geometry.colors = {
                Color(0.05f, 0.75f, 0.95f, 1.0f),
                Color(1.00f, 0.70f, 0.10f, 1.0f)
            };

            e_style = reg.create();
            auto& style = reg.emplace<PolygonStyle>(e_style);
            style.color = StockColor::Lime;
            style.useGeometryColors = true;

            // Local polygons use normalized coordinates. Their Transform
            // scales them to a 7.2 km by 3.6 km footprint and, for overlays,
            // describes an explicit 1 km-deep projector volume.
            auto makeLocalPolygon = [&](double longitude, double latitude, double altitude)
            {
                const auto entity = reg.create();
                reg.emplace<rocky::Polygon>(entity, geometry, style);

                auto& transform = reg.emplace<Transform>(entity);
                transform.position = GeoPoint(
                    SRS::WGS84, longitude, latitude, altitude);
                transform.topocentric = true;
                transform.localMatrix = glm::scale(
                    glm::dmat4(1.0), glm::dvec3(7200.0, 3600.0, 1000.0));
                return entity;
            };

            // Ordinary polygons become meshes. Elevating this one makes the
            // undraped rendering path easy to distinguish from the overlays.
            e_normal = makeLocalPolygon(centerLon, centerLat + 0.075, 1800.0);

            // The same source rings are triangulated for RTT baking.
            e_rtt = makeLocalPolygon(centerLon - 0.065, centerLat - 0.055, 0.0);
            auto& rtt = reg.emplace<Overlay>(e_rtt);
            rtt.mode = OverlayMode::Raster;
            rtt.resolution = { 512u, 512u };

            // construct a vector overlay explicitly:
            e_vector = makeLocalPolygon(centerLon + 0.065, centerLat - 0.055, 0.0);
            auto& vector = reg.emplace<Overlay>(e_vector);
            vector.mode = OverlayMode::Vector;

            // Absolute coordinates need no placement Transform. OverlayBakeSystem
            // will create and maintain a fitted projector from these ring bounds.
            e_autofit_geometry = reg.create();
            auto& autofitGeometry = reg.emplace<PolygonGeometry>(e_autofit_geometry);
            autofitGeometry.srs = SRS::WGS84;
            autofitGeometry.polygons.emplace_back(PolygonPart{
                {
                    { centerLon - 0.030, centerLat - 0.018, 0.0 },
                    { centerLon + 0.030, centerLat - 0.018, 0.0 },
                    { centerLon + 0.030, centerLat + 0.018, 0.0 },
                    { centerLon - 0.030, centerLat + 0.018, 0.0 }
                },
                {{
                    { centerLon - 0.012, centerLat - 0.008, 0.0 },
                    { centerLon - 0.012, centerLat + 0.008, 0.0 },
                    { centerLon + 0.012, centerLat + 0.008, 0.0 },
                    { centerLon + 0.012, centerLat - 0.008, 0.0 }
                }}
            });
            autofitGeometry.colors.emplace_back(
                Color(0.75f, 0.20f, 1.00f, 1.0f));

            e_autofit_rtt = reg.create();
            reg.emplace<rocky::Polygon>(e_autofit_rtt, autofitGeometry, style);
            auto& autofitOverlay = reg.emplace<Overlay>(e_autofit_rtt);
            autofitOverlay.mode = OverlayMode::Raster;
            autofitOverlay.resolution = { 512u, 512u };
        });

        if (auto manip = MapManipulator::get(
            app.display.window(0).view(0).vsgView))
        {
            Viewpoint viewpoint;
            viewpoint.point = GeoPoint(SRS::WGS84, centerLon, centerLat, 0.0);
            viewpoint.range = 35000.0;
            viewpoint.pitch = -65.0;
            manip->setViewpoint(viewpoint, 1.0s);
        }

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped(
        "The north, southwest, and southeast instances share normalized local "
        "geometry with explicit transforms for normal, Raster, and Vector rendering. "
        "The center WGS84 polygon has no Transform; its Raster projector is fitted "
        "automatically from the ring bounds.");

    bool changed = false;
    app.registry.write([&](entt::registry& reg)
    {
        if (ImGuiLTable::Begin("polygon-component"))
        {
            auto visibilityControl = [&](const char* label, entt::entity entity)
            {
                auto& visibility = reg.get<Visibility>(entity);
                if (ImGuiLTable::Checkbox(label, &visibility.visible[0]))
                {
                    visibility.visible.fill(visibility.visible[0]);
                    changed = true;
                }
            };

            visibilityControl("Show normal", e_normal);
            visibilityControl("Show explicit Raster", e_rtt);
            visibilityControl("Show auto-fit Raster", e_autofit_rtt);
            visibilityControl("Show explicit Vector", e_vector);

            auto& style = reg.get<PolygonStyle>(e_style);
            if (ImGuiLTable::Checkbox(
                "Per-polygon colors", &style.useGeometryColors))
            {
                style.dirty(reg);
                changed = true;
            }

            if (ImGuiLTable::ColorEdit3(
                "Shared fill color", reinterpret_cast<float*>(&style.color)))
            {
                style.dirty(reg);
                changed = true;
            }

            auto& geometry = reg.get<PolygonGeometry>(e_geometry);
            if (ImGuiLTable::ColorEdit3(
                "First polygon color",
                reinterpret_cast<float*>(&geometry.colors[0])))
            {
                geometry.dirty(reg);
                changed = true;
            }
            if (ImGuiLTable::ColorEdit3(
                "Second polygon color",
                reinterpret_cast<float*>(&geometry.colors[1])))
            {
                geometry.dirty(reg);
                changed = true;
            }

            auto& autofitGeometry =
                reg.get<PolygonGeometry>(e_autofit_geometry);
            if (ImGuiLTable::ColorEdit3(
                "Auto-fit polygon color",
                reinterpret_cast<float*>(&autofitGeometry.colors[0])))
            {
                autofitGeometry.dirty(reg);
                changed = true;
            }

            ImGuiLTable::End();
        }
    });

    if (changed)
        app.vsgcontext->requestFrame();
};
