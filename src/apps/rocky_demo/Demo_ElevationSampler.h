/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/ElevationSampler.h>
using namespace ROCKY_NAMESPACE;

namespace
{
    //! VSG event handler that captures mouse actions as geopoints.
    class ElevationSamplerMouseHandler : public vsg::Inherit<vsg::Visitor, ElevationSamplerMouseHandler>
    {
    public:
        ElevationSamplerMouseHandler(Application& in_app) : app(in_app) {}
        Callback<void(const GeoPoint&)> onMouseMove;

    protected:
        Application& app;

        Result<GeoPoint> mapPoint(vsg::PointerEvent& e) const
        {
            if (auto p = app.display.pointAtWindowCoords(e.window, e.x, e.y))
                return p;
            else
                return Failure{};
        }

        void apply(vsg::MoveEvent& e) override
        {
            if (auto p = mapPoint(e))
                onMouseMove.fire(p.value());
            else
                onMouseMove.fire(GeoPoint());
        }
    };
}

auto Demo_ElevationSampler = [](Application& app)
{
    static entt::entity entity = entt::null;
    static CallbackSubs subs;
    static std::uint64_t frame = 0;
    static auto active = [](Application& app) {return (app.frameCount() - frame < 2); };
    static ElevationSampler sampler;
    static Future<Result<ElevationSample>> sample;
    static GeoPoint mouse;

    frame = app.viewer->getFrameStamp()->frameCount;

    if(entity == entt::null)
    {
        // make a crosshairs that tracks the clamped mouse position:
        app.registry.write([&](entt::registry& r)
            {
                entity = r.create();
                        
                double t = 500.0;
                auto& geom = r.emplace<LineGeometry>(entity);
                geom.topology = LineTopology::Segments;
                geom.points = { {-t, 0, 0}, {t, 0, 0}, {0, -t, 0}, {0, t, 0}, {0, 0, -t}, {0, 0, t} };

                auto& style = r.emplace<LineStyle>(entity);
                style.color = Color::Cyan;
                style.width = 4.0f;

                r.emplace<Line>(entity, geom, style);

                auto& transform = r.emplace<Transform>(entity);
                transform.topocentric = true;
                transform.frustumCulled = false;
                transform.horizonCulled = false;
            });

        // Configure our sampler.
        sampler.layer = app.mapNode->map->layer<ElevationLayer>();

        // event handler to capture mouse movements:
        auto handler = ElevationSamplerMouseHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        subs += handler->onMouseMove([&](const GeoPoint& p)
            {
                if (p.valid())
                {
                    app.registry.read([&](entt::registry& r)
                        {
                            auto& transform = r.get<Transform>(entity);
                            transform.position = p;
                            transform.dirty(r);
                        });

                    mouse = p.transform(SRS::WGS84);

                    sample = jobs::dispatch([&app, point(p)](Cancelable& c)
                        {
                            return sampler.sample(point, app.io().with(c));
                        });
                }
                else
                {
                    sample.reset();
                }

                app.vsgcontext->requestFrame();
            });

        app.vsgcontext->requestFrame();
    }

    ImGuiLTable::Begin("elevation sampler");

    auto intersection = app.mapNode->terrainNode->intersect(mouse);

    if (intersection)
    {
        ImGuiLTable::Text("Ray intersection", "", "");
        GeoPoint i = intersection->transform(SRS::WGS84);
        ImGuiLTable::Text("WGS84:", "%.2f, %.2f, %.2f", i.x, i.y, i.z);

        // Various coordinate spaces:
        auto world = i.transform(app.mapNode->srs());
        auto camera = app.display.viewAtWindowCoords(app.viewer->windows().front(), 0, 0)->camera;
        auto viewMatrix = camera->viewMatrix->transform();
        auto projMatrix = camera->projectionMatrix->transform();
        auto viewPos = viewMatrix * vsg::dvec4(world.x, world.y, world.z, 1.0);
        auto clipPos = projMatrix * viewPos;
        ImGuiLTable::Text("World:", "%.2lf, %.2lf, %.2lf", world.x, world.y, world.z);
        ImGuiLTable::Text("View:", "%.2lf, %.2lf, %.2lf", viewPos.x / viewPos.w, viewPos.y / viewPos.w, viewPos.z / viewPos.w);
        ImGuiLTable::Text("Clip:", "%.3lf, %.3lf, %.7lf", clipPos.x / clipPos.w, clipPos.y / clipPos.w, clipPos.z / clipPos.w);

        if (sampler.layer)
        {
            ImGui::Separator();

            if (sample.available() && sample.value().ok())
            {
                ImGuiLTable::Text("Elevation sampler:", "%.2f m", sample->value().height);
                ImGuiLTable::Text("Geometric error:", "%.2f m", std::abs(sample->value().height - i.z));
            }
            else if (sample.working())
            {
                ImGuiLTable::Text("Elevation sampler:", "%s", "...");
                ImGuiLTable::Text("Geometric error:", "%s", "...");

                if (sample.working())
                    app.vsgcontext->requestFrame();
            }
            else
            {
                ImGuiLTable::Text("Elevation sampler:", "%s", "no data");
            }
        }
        else
        {
            ImGuiLTable::Text("Elevation sampler:", "%s", "n/a - no elevation layer");
        }
    }
    else
    {
        ImGuiLTable::Text("GeoPoint:", "%s", "no intersection");
    }

    ImGuiLTable::End();
};
