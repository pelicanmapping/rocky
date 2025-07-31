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
            if (auto view = app.display.getView(e.window, e.x, e.y))
            {
                if (auto p = rocky::pointAtWindowCoords(view, e.x, e.y))
                    return p;
            }
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

    //inline bool active(Application& app, int frame) {
    //    return (app.viewer->getFrameStamp()->frameCount - frame < 2);
    //}
}


auto Demo_Draw = [](Application& app)
{
    static entt::entity entity = entt::null;
    static CallbackSubs subs;
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
                auto& line = r.emplace<Line>(entity);
                line.style.color = Color::Yellow;
                line.style.width = 3;
                line.style.depth_offset = 1000;
                line.topology = Line::Topology::Strip;
                line.srs = SRS::ECEF;
                line.staticSize = 1024;
            });

        auto handler = MapEventHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        // left click: start or continue a line:
        subs += handler->onLeftClick([&](const GeoPoint& p)
            {
                if (!active(app)) return;

                app.registry.read([&](entt::registry& r)
                    {
                        auto& line = r.get<Line>(entity);
                        if (!drawing)
                            line.points = { p };
                        line.points.emplace_back(p);
                        line.dirty();
                    });
                drawing = true;
            });

        // move: continue a line:
        subs += handler->onMouseMove([&](const GeoPoint& p)
            {
                if (!active(app)) return;

                if (drawing)
                {
                    app.registry.read([&](entt::registry& r)
                        {
                            auto& line = r.get<Line>(entity);
                            line.points.back() = p;
                            line.dirty();
                        });
                }
            });

        // right click: finish a line:
        subs += handler->onRightClick([&](const GeoPoint& p)
            {
                if (!active(app)) return;

                if (drawing)
                {
                    app.registry.read([&](entt::registry& r)
                        {
                            auto& line = r.get<Line>(entity);
                            line.points.emplace_back(p);
                            line.dirty();
                        });
                }
                drawing = false;
            });
    }

    ImGui::Text("Left click: start a line or add a new point");
    ImGui::Text("Right click: finish a line");

    if (ImGui::Button("Clear"))
    {
        app.registry.read([&](entt::registry& r)
            {
                auto& line = r.get<Line>(entity);
                line.points.clear();
                line.dirty();
            });

        drawing = false;
    }
};
