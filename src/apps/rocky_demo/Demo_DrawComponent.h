/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

/** Example of drawing lines on the map with the mouse */

auto Demo_Draw = [](Application& app)
{
    static entt::entity entity = entt::null;
    static CallbackSubs subs;
    static bool on = false;
    static bool drawing = false;
    static std::uint64_t frame = 0;
    static auto active = [](Application& app) {
            return (app.viewer->getFrameStamp()->frameCount - frame < 2);
        };

    frame = app.viewer->getFrameStamp()->frameCount;

    if (entity == entt::null)
    {
        app.registry.write([&](entt::registry& r)
            {
                entity = r.create();

                auto& geom = r.emplace<LineGeometry>(entity);
                geom.topology = LineTopology::Strip;
                geom.srs = SRS::ECEF;

                auto& style = r.emplace<LineStyle>(entity);
                style.color = StockColor::Yellow;
                style.width = 3;
                style.depthOffset = 1000;
                style.resolution = 1000; // m

                r.emplace<Line>(entity, geom, style);
            });

        auto handler = app.viewer->getObject<GeoMouseHandler>("demo.mouse");

        // left click: start or continue a line:
        subs += handler->onLeftClick([&](const TerrainIntersection& i, const View& view)
            {
                if (!active(app) || !on) return;

                app.registry.write([&](entt::registry& r)
                    {
                        auto&& [geom, style] = r.get<LineGeometry, LineStyle>(entity);

                        if (!drawing)
                            geom.points.clear();
                        geom.points.emplace_back(i.point);
                        geom.dirty(r);
                    });

                drawing = !drawing;

                app.vsgcontext->requestFrame();
            });

        // move: continue a line:
        subs += handler->onMouseMove([&](const TerrainIntersection& i, const View& view)
            {
                if (!active(app) || !on) return;

                if (drawing)
                {
                    app.registry.write([&](entt::registry& r)
                        {
                            auto&& [geom, style] = r.get<LineGeometry, LineStyle>(entity);

                            GeoPoint lastPoint(geom.srs, geom.points.back());
                            auto d = i.point.geodesicDistanceTo(lastPoint).as(Units::METERS);
                            if (d >= style.resolution)
                            {
                                geom.points.emplace_back(i.point);
                                geom.dirty(r);
                            }
                        });
                }
                app.vsgcontext->requestFrame();
            });

        app.vsgcontext->requestFrame();
    }


    app.registry.read([&](entt::registry& r)
        {
            auto&& [geom, style] = r.get<LineGeometry, LineStyle>(entity);

            if (ImGuiLTable::Begin("DrawTable"))
            {
                ImGuiLTable::Checkbox("Draw", &on);

                if (on)
                    ImGuiLTable::TextUnformatted("", "Left-click to start or finish drawing");

                if (ImGuiLTable::SliderFloat("Resolution (m)", &style.resolution, 1000, 100000))
                    style.dirty(r);

                if (ImGuiLTable::SliderFloat("Depth offset (m)", &style.depthOffset, 0.f, 5000.0f))
                    style.dirty(r);

                if (ImGui::Button("Clear"))
                {
                    geom.points.clear();
                    geom.dirty(r);
                    drawing = false;
                    app.vsgcontext->requestFrame();
                };

                ImGuiLTable::End();
            }
        });
};
