/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/vsg/ecs/SlugResource.h>

using namespace ROCKY_NAMESPACE;

// Draws the same mesh, line, and points through both overlay pathways. The RTT
// copy is west of the Slug copy so their output can be compared directly.
auto Demo_SlugOverlay = [](Application& app)
{
    static entt::entity e_rtt = entt::null;
    static entt::entity e_slug = entt::null;
    // Full opacity gives an apples-to-apples comparison for this first slice;
    // grouped overlay opacity is not yet applied as a single post-composite.
    static float opacity = 1.0f;
    static const std::string exportPath =
        (std::filesystem::current_path() / "rocky-slug-segment-repro.slug")
        .lexically_normal().string();

    if (e_slug == entt::null)
    {
        constexpr double centerLon = -122.4207;
        constexpr double centerLat = 37.7732;
        constexpr double spacing = 0.16;
        constexpr double scale = 12000.0;

        auto writer = app.registry.write();
        auto& reg = writer.registry;

        auto e_meshGeometry = reg.create();
        auto& meshGeometry = reg.emplace<MeshGeometry>(e_meshGeometry);
        meshGeometry.vertices = {
            {  0.00,  0.44, 0.0 },
            {  0.14,  0.14, 0.0 },
            {  0.44,  0.10, 0.0 },
            {  0.20, -0.10, 0.0 },
            {  0.28, -0.42, 0.0 },
            {  0.00, -0.24, 0.0 },
            { -0.28, -0.42, 0.0 },
            { -0.20, -0.10, 0.0 },
            { -0.44,  0.10, 0.0 },
            { -0.14,  0.14, 0.0 },
            {  0.00,  0.00, 0.0 }
        };
        meshGeometry.indices = {
            10, 1, 0, 10, 2, 1, 10, 3, 2, 10, 4, 3, 10, 5, 4,
            10, 6, 5, 10, 7, 6, 10, 8, 7, 10, 9, 8, 10, 0, 9
        };

        auto e_meshStyle = reg.create();
        auto& meshStyle = reg.emplace<MeshStyle>(e_meshStyle);
        meshStyle.color = Color(0.05f, 0.65f, 0.95f, 0.85f);
        meshStyle.writeDepth = false;

        auto e_lineGeometry = reg.create();
        auto& lineGeometry = reg.emplace<LineGeometry>(e_lineGeometry);
        // Deliberately match FeatureBuilder's MVT representation: independent
        // segments with bit-identical repeated endpoints. This makes the .slug
        // export a focused reproduction for shared round-cap junctions.
        lineGeometry.topology = LineTopology::Segments;
        lineGeometry.points = {
            { -0.40, -0.30, 0.0 },
            { -0.16,  0.28, 0.0 },
            { -0.16,  0.28, 0.0 },
            {  0.08, -0.12, 0.0 },
            {  0.08, -0.12, 0.0 },
            {  0.38,  0.30, 0.0 }
        };

        auto e_lineStyle = reg.create();
        auto& lineStyle = reg.emplace<LineStyle>(e_lineStyle);
        lineStyle.color = Color(1.0f, 0.15f, 0.75f, 1.0f);
        lineStyle.width = 160.0f;
        lineStyle.widthUnits = Units::METERS;
        lineStyle.outlineWidth = 20.0f;
        lineStyle.outlineColor = Color(0.0f, 0.0f, 0.0f, 1.0f);

        auto e_pointGeometry = reg.create();
        auto& pointGeometry = reg.emplace<PointGeometry>(e_pointGeometry);
        pointGeometry.points = {
            { -0.32, 0.32, 0.0 },
            {  0.00, 0.34, 0.0 },
            {  0.32, 0.32, 0.0 },
            { -0.32,-0.32, 0.0 },
            {  0.32,-0.32, 0.0 }
        };

        auto e_pointStyle = reg.create();
        auto& pointStyle = reg.emplace<PointStyle>(e_pointStyle);
        pointStyle.color = Color(1.0f, 0.9f, 0.05f, 1.0f);
        pointStyle.width = 13.0f;

        auto makeOverlay = [&](double longitude, OverlayTechnique technique)
        {
            auto entity = reg.create();
            reg.emplace<Mesh>(entity, meshGeometry, meshStyle);
            reg.emplace<Line>(entity, lineGeometry, lineStyle);
            reg.emplace<Point>(entity, pointGeometry, pointStyle);

            auto& transform = reg.emplace<Transform>(entity);
            transform.position = GeoPoint(SRS::WGS84, longitude, centerLat, 0.0);
            transform.topocentric = true;
            transform.localMatrix = glm::scale(glm::dmat4(1.0), glm::dvec3(scale));

            auto& overlay = reg.emplace<Overlay>(entity);
            overlay.technique = technique;
            overlay.color.a = opacity;
            overlay.textureSize = { 512u, 512u };
            return entity;
        };

        e_rtt = makeOverlay(centerLon - spacing * 0.5, OverlayTechnique::RTT);
        e_slug = makeOverlay(centerLon + spacing * 0.5, OverlayTechnique::Slug);

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = GeoPoint(SRS::WGS84, centerLon, centerLat, 0.0);
            vp.range = scale * 20.0;
            vp.pitch = -55.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    bool opacityChanged = false;
    bool exportRequested = false;
    app.registry.read([&](entt::registry& reg)
    {
        if (ImGuiLTable::Begin("slug-overlay"))
        {
            ImGuiLTable::Text("West", "RTT");
            ImGuiLTable::Text("East", "Slug");
            if (const auto* resource = reg.try_get<SlugResource>(e_slug))
            {
                if (resource->ready)
                {
                    ImGuiLTable::Text("Slug status", "Ready (%zu layers)", resource->layers.size());
                    exportRequested = ImGuiLTable::Button("Export segment repro (.slug)");
                }
                else
                    ImGuiLTable::Text("Slug status", "%s", resource->message.c_str());

                if (!resource->exportMessage.empty())
                    ImGuiLTable::TextUnformatted("Export status", resource->exportMessage.c_str());
            }
            else
            {
                ImGuiLTable::Text("Slug status", "Waiting for atlas");
            }
            ImGuiLTable::TextUnformatted("Export path", exportPath.c_str());
            opacityChanged = ImGuiLTable::SliderFloat(
                "Opacity", &opacity, 0.0f, 1.0f, "%.2f");

            auto& rttVisibility = reg.get<Visibility>(e_rtt);
            ImGuiLTable::Checkbox("Show RTT", &rttVisibility.visible[0]);
            rttVisibility.visible.fill(rttVisibility.visible[0]);

            auto& slugVisibility = reg.get<Visibility>(e_slug);
            ImGuiLTable::Checkbox("Show Slug", &slugVisibility.visible[0]);
            slugVisibility.visible.fill(slugVisibility.visible[0]);

            ImGuiLTable::End();
        }
    });

    if (exportRequested)
    {
        app.registry.write([&](entt::registry& reg)
        {
            if (auto* resource = reg.try_get<SlugResource>(e_slug))
            {
                resource->exportPath = exportPath;
                resource->exportSucceeded = false;
                resource->exportMessage = "Queued";
            }
        });
        app.vsgcontext->requestFrame();
    }

    if (opacityChanged)
    {
        app.registry.write([&](entt::registry& reg)
        {
            for (auto entity : { e_rtt, e_slug })
            {
                auto& overlay = reg.get<Overlay>(entity);
                overlay.color.a = opacity;
                overlay.dirty(reg);
            }
        });
        app.vsgcontext->requestFrame();
    }
};
