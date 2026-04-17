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

        Callback<const GeoPoint&> onLeftClick;
        Callback<const GeoPoint&> onRightClick;
        Callback<const GeoPoint&> onMouseMove;

    protected:
        Application& app;
        std::optional<vsg::ButtonPressEvent> _press[4];

        Result<GeoPoint> mapPoint(vsg::PointerEvent& e) const
        {
            if (auto& window = app.display.find(e.window.ref_ptr()))
            {
                if (auto& view = window.viewAtCoords(e.x, e.y))
                {
                    return geoPointAtWindowCoords(view, e.x, e.y);
                }
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
}


auto Demo_Draw = [](Application& app)
{
    static entt::entity entity = entt::null;
    static CallbackSubs subs;
    static bool on = false;
    static bool drawing = false;
    static std::uint64_t frame = 0;
    static Feature feature;
    static ElevationSession clamper;
    static auto active = [](Application& app) {
            return (app.viewer->getFrameStamp()->frameCount - frame < 2);
        };

    frame = app.viewer->getFrameStamp()->frameCount;

    if (entity == entt::null)
    {
        feature.geometry.type = Geometry::Type::LineString;

        app.registry.write([&](entt::registry& r)
            {
                entity = r.create();

                auto& geom = r.emplace<LineGeometry>(entity);
                geom.topology = LineTopology::Strip;
                geom.srs = SRS::ECEF;

                auto& style = r.emplace<LineStyle>(entity);
                style.color = StockColor::Yellow;
                style.width = 3;
                style.depthOffset = 20000;
                style.resolution = 10000; // m

                r.emplace<Line>(entity, geom, style);
            });

        auto handler = MapEventHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        // left click: start or continue a line:
        subs += handler->onLeftClick([&](const GeoPoint& p)
            {
                if (!active(app) || !on) return;

                app.registry.write([&](entt::registry& r)
                    {
                        auto&& [geom, style] = r.get<LineGeometry, LineStyle>(entity);

                        if (!drawing)
                        {
                            feature.srs = p.srs;
                            feature.geometry.points.clear();
                        }

                        feature.geometry.points.emplace_back(p);

                        geom.points.clear();
                        FeatureView::generateLine(feature, style, {}, clamper, geom.srs, geom);
                        geom.dirty(r);
                    });

                drawing = !drawing;

                app.vsgcontext->requestFrame();
            });

        // move: continue a line:
        subs += handler->onMouseMove([&](const GeoPoint& p)
            {
                if (!active(app) || !on) return;

                if (drawing)
                {
                    app.registry.write([&](entt::registry& r)
                        {
                            auto&& [geom, style] = r.get<LineGeometry, LineStyle>(entity);

                            GeoPoint lastPoint(feature.srs, feature.geometry.points.back());
                            auto d = p.geodesicDistanceTo(lastPoint).as(Units::METERS);
                            if (d >= style.resolution)
                            {
                                feature.geometry.points.emplace_back(p);
                                geom.points.clear();
                                FeatureView::generateLine(feature, style, {}, clamper, geom.srs, geom);
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

                if (ImGuiLTable::SliderFloat("Resolution (m)", &style.resolution, 5000, 100000))
                    style.dirty(r);

                if (ImGui::Button("Clear"))
                {
                    feature.geometry.points.clear();
                    geom.points.clear();
                    geom.dirty(r);
                    drawing = false;
                    app.vsgcontext->requestFrame();
                };

                ImGuiLTable::End();
            }
        });
};
