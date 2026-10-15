/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <cmath>

using namespace ROCKY_NAMESPACE;


auto Demo_Clamping = [](Application& app)
{
    struct Clamping : public Component<Clamping>
    {
        bool clampToTerrain = true;
    };


    // This demo keeps geometry in geodetic coordinates, so clamping only changes altitude.
    class ClampingSystem : public System
    {
    public:
        //! Watches resident terrain changes affecting this fixed line; the terrain is not owned by the system.
        ClampingSystem(Registry& registry, TerrainNode* terrain, const GeoExtent& extent) :
            System(registry),
            _terrain(terrain)
        {
            _subs += terrain->onTileBoundsChanged([this, extent](const TileKey& key)
            {
                // Includes refinement, elevation replacement, page-out, and reset. Callbacks run at update safe points.
                if (!key.valid() || key.extent().intersects(extent))
                    dirty();
            });
        }

        //! Re-clamps at update time against resident triangles, preserving XY and leaving missed vertices unchanged.
        void update(VSGContext vsgcontext) override
        {
            auto terrain = _terrain.ref_ptr();
            if (!_dirty || !terrain || !terrain->renderingSRS.valid())
                return;

            _dirty = false;
            auto [_, reg] = _registry.write();

            reg.view<LineGeometry, Clamping>().each([&](auto& geom, const auto& clamping)
            {
                if (!clamping.clampToTerrain)
                    return;

                bool changed = false;
                for (auto& point : geom.points)
                {
                    auto hit = terrain->intersectVertical(GeoPoint(geom.srs, point));
                    if (hit.failed())
                        continue;

                    auto resolved = hit.value().point.transform(geom.srs);
                    if (resolved.valid() && std::isfinite(resolved.z) && std::abs(resolved.z - point.z) > 0.001)
                    {
                        point.z = resolved.z;
                        changed = true;
                    }
                }

                if (changed)
                {
                    geom.dirty(reg);
                    // The line renderer may already have updated this frame; schedule its geometry refresh.
                    vsgcontext->requestFrame();
                }
            });
        }

        //! Schedules one clamp pass, coalescing terrain notifications until the next update.
        void dirty()
        {
            _dirty = true;
        }

    private:
        vsg::observer_ptr<TerrainNode> _terrain;
        bool _dirty = true; // Clamp on creation even when terrain finished loading before the demo opened.
        // Declared last so callbacks are disconnected before the state they access is destroyed.
        CallbackSubs _subs;
    };


    static entt::entity e = entt::null;
    static std::shared_ptr<ClampingSystem> sys;

    // The demo runs immediately after its collapsing header, so detect reopening
    // before drawing any other ImGui items. Do not move the camera every frame.
    const bool focusAOI = e == entt::null || ImGui::IsItemToggledOpen();

    if (e == entt::null)
    {
        //! Tessellates the Alpine test line in WGS84 so terrain updates preserve its horizontal coordinates.
        auto rebuildGeom = [](LineGeometry& geom, const LineStyle& style)
        {
            Feature feature;
            feature.geometry.type = Geometry::Type::LineString;
            feature.geometry.points = {
                { 7.49, 46.12, 4000.0 },
                { 8.11, 46.17, 4000.0 }
            };
            FeatureBuilder fb;
            fb.outputSRS = SRS::WGS84;
            fb.buildLineGeometry({ feature }, style, geom);
        };

        GeoExtent extent(SRS::WGS84);
        app.registry.write([&](entt::registry& reg)
            {
                e = reg.create();

                auto& geom = reg.emplace<LineGeometry>(e);

                auto& style = reg.emplace<LineStyle>(e);
                style.color = StockColor::Yellow;
                style.width = 5.0f; // pixels
                style.depthOffset = 100.0f; // meters
                style.resolution = 1000.0f; // meters

                reg.emplace<Line>(e, geom, style);

                auto& clamping = reg.emplace<Clamping>(e);
                clamping.clampToTerrain = true;

                rebuildGeom(geom, style);
                for (const auto& point : geom.points)
                    extent.expandToInclude(point.x, point.y);
            });

        sys = std::make_shared<ClampingSystem>(app.registry, app.mapNode->terrainNode.get(), extent);
        app.systemsNode->add(sys);
    }


    if (focusAOI)
    {
        if (auto manip = MapManipulator::get(app.display.window(0).view(0).vsgView))
        {
            Viewpoint vp;
            vp.point = GeoPoint(SRS::WGS84, 7.80, 46.145, 0.0); // midpoint of the Alpine test line
            vp.range = 80000.0;
            vp.pitch = -45.0;
            vp.heading = 0.0;
            manip->setViewpoint(vp, 1.0s);
        }
    }

    if (ImGuiLTable::Begin("clamping demo"))
    {
        auto [_, reg] = app.registry.read();
        if (ImGuiLTable::Checkbox("Clamp to terrain", &reg.get<Clamping>(e).clampToTerrain))
            sys->dirty();
        ImGuiLTable::Text("Surface", "Automatically follows loaded terrain");
        ImGuiLTable::End();
    }
};
