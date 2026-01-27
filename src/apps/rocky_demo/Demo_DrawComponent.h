/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

namespace
{
    /** Example of draw lines on the map with the mouse */

    //! VSG event handler that captures mouse actions as geopoints.
    class MapEventHandler : public vsg::Inherit<vsg::Visitor, MapEventHandler>
    {
    public:
        MapEventHandler(Application& in_app) : app(in_app) {}

        Callback<void(const GeoPoint&)> onLeftClick;
        Callback<void(const GeoPoint&)> onRightClick;
        Callback<void(const GeoPoint&)> onMouseMove;

    protected:
        Application& app;
        std::optional<vsg::ButtonPressEvent> _press[4];

        Result<GeoPoint> mapPoint(vsg::PointerEvent& e) const
        {
            if (auto p = app.display.pointAtWindowCoords(e.window, e.x, e.y))
                return p;
            else
                return Failure{};
        }

        void apply(vsg::ButtonPressEvent& e) override
        {
            _press[e.button] = e;
        }

        void apply(vsg::ButtonReleaseEvent& e) override
        {
            auto& press = _press[e.button];
            if (press.has_value())
            {
                auto dx = std::abs(e.x - press->x), dy = std::abs(e.y - press->y);
                if (dx < 5 && dy < 5) // click threshold
                {                    
                    if (auto p = mapPoint(press.value()))
                    {
                        if (e.button == 1)
                            onLeftClick.fire(p.value());
                        else if (e.button == 3)
                            onRightClick.fire(p.value());
                    }
                }
            }
            press.reset();
        }

        void apply(vsg::MoveEvent& e) override
        {
            if (auto p = mapPoint(e))
            {
                onMouseMove.fire(p.value());
            }
        }
    };
}


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
                geom.points.reserve(4);

                auto& style = r.emplace<LineStyle>(entity);
                style.color = Color::Yellow;
                style.width = 3;
                style.depthOffset = 20000;

                r.emplace<Line>(entity, geom, style);
            });

        auto handler = MapEventHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        // left click: start or continue a line:
        subs += handler->onLeftClick([&](const GeoPoint& p)
            {
                if (!active(app)) return;
                if (!on) return;

                app.registry.read([&](entt::registry& r)
                    {
                        auto& geom = r.get<LineGeometry>(entity);
                        if (!drawing)
                            geom.points = { p };
                        geom.points.emplace_back(p);
                        geom.dirty(r);
                    });
                drawing = true;
                app.vsgcontext->requestFrame();
            });

        // move: continue a line:
        subs += handler->onMouseMove([&](const GeoPoint& p)
            {
                if (!active(app)) return;

                if (drawing)
                {
                    app.registry.read([&](entt::registry& r)
                        {
                            auto& geom = r.get<LineGeometry>(entity);
                            geom.points.back() = p;
                            geom.dirty(r);
                        });
                }
                app.vsgcontext->requestFrame();
            });

        // right click: finish a line:
        subs += handler->onRightClick([&](const GeoPoint& p)
            {
                if (!active(app)) return;

                if (drawing)
                {
                    app.registry.read([&](entt::registry& r)
                        {
                            auto& geom = r.get<LineGeometry>(entity);
                            geom.points.emplace_back(p);
                            geom.dirty(r);
                        });
                }
                drawing = false;
                on = false;
                app.vsgcontext->requestFrame();
            });

        app.vsgcontext->requestFrame();
    }

    ImGui::Text("%s", "Left click: start a line or add a new point");
    ImGui::Text("%s", "Right click: finish a line");

    ImGui::Checkbox("Draw", &on);
    ImGui::SameLine();

    if (ImGui::Button("Clear"))
    {
        app.registry.read([&](entt::registry& r)
            {
                auto& geom = r.get<LineGeometry>(entity);
                geom.points.clear();
                geom.dirty(r);
            });

        drawing = false;
        app.vsgcontext->requestFrame();
    }
};
