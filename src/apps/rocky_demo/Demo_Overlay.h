/**
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#include <algorithm>

using namespace ROCKY_NAMESPACE;

auto Demo_Overlay_Relative = [](Application& app)
{
    static entt::entity e_overlay = entt::null;
    static entt::entity e_mesh = entt::null;
    static const double startLon = -122.4207;
    static const double startLat = 37.677;
    static double scale = 10000.0;
    static float opacity = 0.85f;
    static bool useOverlay = true;
    static bool useDepthBuffer = false;
    static bool continuousBake = false;

    if (e_overlay == entt::null)
    {
        auto& io = app.io();
        auto r = io.services().readImageFromURI(TEXTURE_GRID, io);
        Image::Ptr image;
        if (r.ok())
            image = r.value();

        auto&& [_, reg] = app.registry.write();

        auto e_meshGeom = reg.create();
        auto& meshGeom = reg.emplace<MeshGeometry>(e_meshGeom);
        meshGeom.vertices = {
            {  0.000,  0.000, 0.0 },
            {  0.000,  0.500, 0.0 },
            {  0.118,  0.162, 0.0 },
            {  0.475,  0.154, 0.0 },
            {  0.190, -0.062, 0.0 },
            {  0.294, -0.405, 0.0 },
            {  0.000, -0.200, 0.0 },
            { -0.294, -0.405, 0.0 },
            { -0.190, -0.062, 0.0 },
            { -0.475,  0.154, 0.0 },
            { -0.118,  0.162, 0.0 }
        };
        meshGeom.normals = {
            { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 },
            { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 },
            { 0, 0, 1 }
        };
        meshGeom.uvs = {
            { 0.500, 0.500 },
            { 0.500, 1.000 },
            { 0.618, 0.662 },
            { 0.975, 0.654 },
            { 0.690, 0.438 },
            { 0.794, 0.095 },
            { 0.500, 0.300 },
            { 0.206, 0.095 },
            { 0.310, 0.438 },
            { 0.025, 0.654 },
            { 0.382, 0.662 }
        };
        meshGeom.indices = {
            0, 2, 1,
            0, 3, 2,
            0, 4, 3,
            0, 5, 4,
            0, 6, 5,
            0, 7, 6,
            0, 8, 7,
            0, 9, 8,
            0, 10, 9,
            0, 1, 10
        };

        auto e_meshStyle = reg.create();
        auto& meshStyle = reg.emplace<MeshStyle>(e_meshStyle);
        meshStyle.color = Color(1.0f, 1.0f, 1.0f, 1.0f);

        if (image)
        {
            auto e_meshTex = reg.create();
            auto& meshTex = reg.emplace<MeshTexture>(e_meshTex);
            meshTex.imageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), moveImageToVSG(image));
            meshStyle.texture = e_meshTex;
        }

        e_mesh = reg.create();
        reg.emplace<Mesh>(e_mesh, meshGeom, meshStyle);

        auto& meshXform = reg.emplace<Transform>(e_mesh);
        meshXform.position = GeoPoint(SRS::WGS84, startLon, startLat, 0.0);
        meshXform.topocentric = true;
        meshXform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));

        if (useOverlay)
        {
            auto& overlay = reg.emplace<Overlay>(e_mesh);
            overlay.color.a = opacity;
            overlay.continuousBake = continuousBake;
        }
        e_overlay = e_mesh;

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = meshXform.position;
            vp.range = scale * 20.0;
            vp.pitch = -35.0;
            vp.heading = 20.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    bool toggleOverlay = false;
    bool nextOverlay = useOverlay;

    app.registry.read([&](entt::registry& reg)
    {
        if (ImGuiLTable::Begin("overlay-draped-mesh"))
        {
            if (ImGuiLTable::Checkbox("Use overlay", &nextOverlay))
                toggleOverlay = true;

            auto& overlayVisibility = reg.get<Visibility>(e_overlay);
            if (ImGuiLTable::Checkbox("Show", &overlayVisibility.visible[0]))
            {
                overlayVisibility.visible.fill(overlayVisibility.visible[0]);
            }

            bool hasOverlay = reg.any_of<Overlay>(e_overlay);
            float alpha = opacity;
            if (hasOverlay)
            {
                const auto& overlay = reg.get<Overlay>(e_overlay);
                alpha = overlay.color.a;
                useDepthBuffer = overlay.useDepthBuffer;
            }

            if (ImGuiLTable::SliderFloat("Opacity", &alpha, 0.0f, 1.0f, "%.2f"))
            {
                opacity = alpha;
                if (hasOverlay)
                {
                    auto& overlay = reg.get<Overlay>(e_overlay);
                    overlay.color.a = alpha;
                    overlay.dirty(reg);
                }
            }

            if (ImGuiLTable::Checkbox("Continuous bake", &continuousBake) && hasOverlay)
            {
                reg.get<Overlay>(e_overlay).continuousBake = continuousBake;
                app.vsgcontext->requestFrame();
            }

            if (ImGuiLTable::Checkbox("Depth buffer", &useDepthBuffer) && hasOverlay)
            {
                reg.get<Overlay>(e_overlay).useDepthBuffer = useDepthBuffer;
                app.vsgcontext->requestFrame();
            }

            auto& overlayXform = reg.get<Transform>(e_overlay);
            auto& meshXform = reg.get<Transform>(e_mesh);

            bool moved = false;
            moved = ImGuiLTable::SliderDouble("Latitude", &overlayXform.position.y, startLat - 1.0, startLat + 1.0, "%.4lf") || moved;
            moved = ImGuiLTable::SliderDouble("Longitude", &overlayXform.position.x, startLon - 1.0, startLon + 1.0, "%.4lf") || moved;

            if (moved)
            {
                meshXform.position = overlayXform.position;
                overlayXform.dirty(reg);
                meshXform.dirty(reg);
            }

            auto rot = quaternion_from_matrix<glm::dquat>(overlayXform.localMatrix);
            auto [pitch, roll, heading] = euler_degrees_from_quaternion(rot);

            bool changed = false;
            changed = ImGuiLTable::SliderDouble("Scale", &scale, 1000.0, 100000.0, "%.1lf", ImGuiSliderFlags_Logarithmic) || changed;
            changed = ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf") || changed;

            if (changed)
            {
                auto q = quaternion_from_euler_degrees(pitch, roll, heading);
                auto local = glm::mat4_cast(q) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                overlayXform.localMatrix = local;
                meshXform.localMatrix = local;
                overlayXform.dirty(reg);
                meshXform.dirty(reg);
            }

            ImGuiLTable::End();
        }
    });

    if (toggleOverlay)
    {
        useOverlay = nextOverlay;
        app.registry.write([&](entt::registry& r)
        {
            if (useOverlay)
            {
                auto& overlay = r.emplace_or_replace<Overlay>(e_mesh);
                overlay.color.a = opacity;
                overlay.useDepthBuffer = useDepthBuffer;
                overlay.continuousBake = continuousBake;
                overlay.dirty(r);
            }
            else
                r.remove<Overlay>(e_mesh);
        });
    }
};


auto Demo_Overlay_Absolute = [](Application& app)
{
    static entt::entity e = entt::null;
    static double opacity = 0.80;
    static bool useOverlay = true;
    static bool useDepthBuffer = false;
    static float depthSafetyFactorUI = 1.0f;
    static bool continuousBake = false;

    if (e == entt::null)
    {
        auto&& [_, reg] = app.registry.write();

        const double centerLon = -122.4207;
        const double centerLat = 37.7732;
        const double lonSpan = 0.18;
        const double latSpan = 0.14;

        e = reg.create();
        auto& meshGeom = reg.emplace<MeshGeometry>(e);
        meshGeom.srs = SRS::WGS84;

        meshGeom.vertices = {
            { centerLon +  0.000 * lonSpan, centerLat +  0.000 * latSpan, 0.0 },
            { centerLon +  0.000 * lonSpan, centerLat +  0.500 * latSpan, 0.0 },
            { centerLon +  0.118 * lonSpan, centerLat +  0.162 * latSpan, 0.0 },
            { centerLon +  0.475 * lonSpan, centerLat +  0.154 * latSpan, 0.0 },
            { centerLon +  0.190 * lonSpan, centerLat + -0.062 * latSpan, 0.0 },
            { centerLon +  0.294 * lonSpan, centerLat + -0.405 * latSpan, 0.0 },
            { centerLon +  0.000 * lonSpan, centerLat + -0.200 * latSpan, 0.0 },
            { centerLon + -0.294 * lonSpan, centerLat + -0.405 * latSpan, 0.0 },
            { centerLon + -0.190 * lonSpan, centerLat + -0.062 * latSpan, 0.0 },
            { centerLon + -0.475 * lonSpan, centerLat +  0.154 * latSpan, 0.0 },
            { centerLon + -0.118 * lonSpan, centerLat +  0.162 * latSpan, 0.0 }
        };

        meshGeom.normals = {
            { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 },
            { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 },
            { 0, 0, 1 }
        };

        meshGeom.uvs = {
            { 0.500, 0.500 },
            { 0.500, 1.000 },
            { 0.618, 0.662 },
            { 0.975, 0.654 },
            { 0.690, 0.438 },
            { 0.794, 0.095 },
            { 0.500, 0.300 },
            { 0.206, 0.095 },
            { 0.310, 0.438 },
            { 0.025, 0.654 },
            { 0.382, 0.662 }
        };

        meshGeom.indices = {
            0, 2, 1,
            0, 3, 2,
            0, 4, 3,
            0, 5, 4,
            0, 6, 5,
            0, 7, 6,
            0, 8, 7,
            0, 9, 8,
            0, 10, 9,
            0, 1, 10
        };

        auto& meshStyle = reg.emplace<MeshStyle>(e);
        meshStyle.color = Color(0.1f, 0.9f, 1.0f, 1.0f);
        meshStyle.depthOffset = 200.0f;
        reg.emplace<Mesh>(e, meshGeom, meshStyle);

        auto& optics = reg.emplace<Optics>(e);
        optics.projection = Optics::Projection::Orthographic;
        optics.autoComputeFocalDistance = true;

        auto& lineGeom = reg.emplace<LineGeometry>(e);
        lineGeom.srs = SRS::WGS84;
        lineGeom.points = {
            { centerLon +  0.000 * lonSpan, centerLat +  0.500 * latSpan, 0.0 },
            { centerLon +  0.118 * lonSpan, centerLat +  0.162 * latSpan, 0.0 },
            { centerLon +  0.475 * lonSpan, centerLat +  0.154 * latSpan, 0.0 },
            { centerLon +  0.190 * lonSpan, centerLat + -0.062 * latSpan, 0.0 },
            { centerLon +  0.294 * lonSpan, centerLat + -0.405 * latSpan, 0.0 },
            { centerLon +  0.000 * lonSpan, centerLat + -0.200 * latSpan, 0.0 },
            { centerLon + -0.294 * lonSpan, centerLat + -0.405 * latSpan, 0.0 },
            { centerLon + -0.190 * lonSpan, centerLat + -0.062 * latSpan, 0.0 },
            { centerLon + -0.475 * lonSpan, centerLat +  0.154 * latSpan, 0.0 },
            { centerLon + -0.118 * lonSpan, centerLat +  0.162 * latSpan, 0.0 },
            { centerLon +  0.000 * lonSpan, centerLat +  0.500 * latSpan, 0.0 }
        };

        auto& lineStyle = reg.emplace<LineStyle>(e);
        lineStyle.color = Color(1.0f, 0.1f, 0.9f, 1.0f);
        lineStyle.width = 5.0f;
        lineStyle.depthOffset = 200.0f;
        lineStyle.stipplePattern = 0x00FF;

        reg.emplace<Line>(e, lineGeom, lineStyle);

        auto& pointGeom = reg.emplace<PointGeometry>(e);
        pointGeom.srs = SRS::WGS84;
        pointGeom.points.reserve(25);
        const double ringLon = lonSpan * 0.35;
        const double ringLat = latSpan * 0.35;
        const double twoPi = 2.0 * std::acos(-1.0);
        for (int i = 0; i < 25; ++i)
        {
            double a = (twoPi * (double)i) / 25.0;
            pointGeom.points.emplace_back(
                centerLon + cos(a) * ringLon,
                centerLat + sin(a) * ringLat,
                0.0);
        }

        auto& pointStyle = reg.emplace<PointStyle>(e);
        pointStyle.color = Color(1.0f, 1.0f, 0.1f, 1.0f);
        pointStyle.width = 7.0f;
        pointStyle.depthOffset = 200.0f;

        reg.emplace<Point>(e, pointGeom, pointStyle);

        double eastMeters = GeoPoint(SRS::WGS84, centerLon, centerLat, 0.0)
            .geodesicDistanceTo(GeoPoint(SRS::WGS84, centerLon + lonSpan, centerLat, 0.0))
            .as(Units::METERS);
        double northMeters = GeoPoint(SRS::WGS84, centerLon, centerLat, 0.0)
            .geodesicDistanceTo(GeoPoint(SRS::WGS84, centerLon, centerLat + latSpan, 0.0))
            .as(Units::METERS);

        if (useOverlay)
        {
            auto& overlay = reg.emplace<Overlay>(e);
            overlay.color.a = (float)opacity;
            overlay.continuousBake = continuousBake;
        }

        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = GeoPoint(SRS::WGS84, centerLon, centerLat, 0.0);
            vp.range = std::max(eastMeters, northMeters) * 12.0;
            vp.pitch = -35.0;
            vp.heading = 20.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    bool toggleOverlay = false;
    bool nextOverlay = useOverlay;
    bool setAutoClamp = false;
    bool autoClamp = true;

    app.registry.read([&](entt::registry& reg)
    {
        if (ImGuiLTable::Begin("overlay-draped-mesh-absolute"))
        {
            if (ImGuiLTable::Checkbox("Use overlay", &nextOverlay))
                toggleOverlay = true;

            auto& overlayVisibility = reg.get<Visibility>(e);
            if (ImGuiLTable::Checkbox("Show", &overlayVisibility.visible[0]))
            {
                overlayVisibility.visible.fill(overlayVisibility.visible[0]);
            }

            float alpha = (float)opacity;
            if (reg.any_of<Overlay>(e))
            {
                const auto& overlay = reg.get<Overlay>(e);
                alpha = overlay.color.a;
                useDepthBuffer = overlay.useDepthBuffer;
            }

            if (auto* optics = reg.try_get<Optics>(e))
                autoClamp = optics->autoComputeFocalDistance;

            if (ImGuiLTable::SliderFloat("Opacity", &alpha, 0.0f, 1.0f, "%.2f"))
            {
                opacity = alpha;
                if (reg.any_of<Overlay>(e))
                {
                    auto& overlay = reg.get<Overlay>(e);
                    overlay.color.a = alpha;
                    overlay.dirty(reg);
                }
            }

            if (ImGuiLTable::Checkbox("Auto clamp center", &autoClamp))
                setAutoClamp = true;

            if (ImGuiLTable::Checkbox("Continuous bake", &continuousBake) && reg.any_of<Overlay>(e))
            {
                reg.get<Overlay>(e).continuousBake = continuousBake;
                app.vsgcontext->requestFrame();
            }

            if (ImGuiLTable::Checkbox("Depth buffer", &useDepthBuffer) && reg.any_of<Overlay>(e))
            {
                reg.get<Overlay>(e).useDepthBuffer = useDepthBuffer;
                app.vsgcontext->requestFrame();
            }

            if (auto* overlayBake = app.computeSystemsNode ? app.computeSystemsNode->get<OverlayBakeSystemNode>() : nullptr)
            {
                depthSafetyFactorUI = overlayBake->depthSafetyFactor;
                if (ImGuiLTable::SliderFloat("Depth safety", &depthSafetyFactorUI, 0.25f, 3.0f, "%.2f"))
                {
                    overlayBake->depthSafetyFactor = depthSafetyFactorUI;
                    app.vsgcontext->requestFrame();
                }
            }

            ImGuiLTable::End();
        }
    });

    if (toggleOverlay)
    {
        useOverlay = nextOverlay;
        app.registry.write([&](entt::registry& r)
        {
            if (useOverlay)
            {
                auto& overlay = r.emplace_or_replace<Overlay>(e);
                overlay.color.a = (float)opacity;
                overlay.useDepthBuffer = useDepthBuffer;
                overlay.continuousBake = continuousBake;
                overlay.dirty(r);
            }
            else
                r.remove<Overlay>(e);
        });
    }

    if (setAutoClamp)
    {
        app.registry.write([&](entt::registry& r)
        {
            if (auto* optics = r.try_get<Optics>(e))
                optics->autoComputeFocalDistance = autoClamp;
        });
        app.vsgcontext->requestFrame();
    }

};
