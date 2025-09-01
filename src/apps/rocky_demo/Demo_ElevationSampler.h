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
            if (auto view = app.display.getView(e.window, e.x, e.y))
            {
                if (auto p = rocky::pointAtWindowCoords(view, e.x, e.y))
                    return p;
            }
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
    static jobs::future<Result<ElevationSample>> sample;
    static GeoPoint mouse;

    frame = app.viewer->getFrameStamp()->frameCount;

    if(entity == entt::null)
    {
        // make a crosshairs that tracks the clamped mouse position:
        app.registry.write([&](entt::registry& r)
            {
                entity = r.create();
                        
                auto& line = r.emplace<Line>(entity);
                line.style.color = Color::Cyan;
                line.style.width = 4.0f;
                line.topology = Line::Topology::Segments;
                double t = 500.0;
                line.points = { {-t, 0, 0}, {t, 0, 0}, {0, -t, 0}, {0, t, 0}, {0, 0, -t}, {0, 0, t} };

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
                            transform.dirty();
                        });

                    mouse = p.transform(SRS::WGS84);

                    sample = jobs::dispatch([&app, point(p)](Cancelable& c)
                        {
                            return sampler.sample(point, IOOptions(app.io(), c));
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

    ImGuiLTable::Text("Point at mouse:", "%.2f, %.2f, %.2f", mouse.x, mouse.y, mouse.z);

    auto intersection = app.mapNode->terrainNode->intersect(mouse);

    if (intersection)
    {
        GeoPoint i = intersection->transform(SRS::WGS84);
        ImGuiLTable::Text("Mesh intersection:", "%.2f, %.2f, %.2f", i.x, i.y, i.z);
    }
    else
    {
        ImGuiLTable::Text("Mesh intersection:", "---");
    }

    if (sampler.layer)
    {
        if (sample.available() && sample.value().ok())
        {
            ImGuiLTable::Text("Elevation sampler:", "%.2f m", sample->value());

        }
        else if (sample.working())
        {
            ImGuiLTable::Text("Elevation sampler:", "...");

            if (sample.working())
                app.vsgcontext->requestFrame();
        }
        else
        {
            ImGuiLTable::Text("Elevation sampler:", "no data");
        }
    }
    else
    {
        ImGuiLTable::Text("Elevation sampler:", "n/a - no elevation layer");
    }
    ImGuiLTable::End();
};
