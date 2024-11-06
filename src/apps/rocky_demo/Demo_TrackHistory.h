/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>
#include <rocky/vsg/ECS.h>
#include "helpers.h"
#include <chrono>

using namespace ROCKY_NAMESPACE;
using namespace std::chrono_literals;

namespace
{
    const int track_chunk_size = 16;

    // ECS Component
    struct TrackHistory
    {
        struct Chunk
        {
            GeoPoint referencePoint;
            entt::entity attach_point = entt::null; // attachment point for the line
            std::vector<std::chrono::steady_clock::time_point> timestamps;
            std::vector<vsg::dvec3> points; // in the Line reference point's SRS
            //std::vector<vsg::dmat4> localMatrices;
        };

        LineStyle style;
        std::vector<Chunk> chunks;
    };

    class TrackHistorySystem : public ECS::System
    {
    public:
        TrackHistorySystem(entt::registry& registry) : System(registry) {}
        static auto create(entt::registry& r) { return std::make_shared<TrackHistorySystem>(r); }

        std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();
        float update_hertz = 1.0f; // updates per second

        bool visible = true;

        void update(Runtime& runtime) override
        {
            auto now = std::chrono::steady_clock::now();
            auto freq = 1s / update_hertz;
            auto elapsed = (now - last_update);

            if (elapsed >= freq)
            {
                auto view = registry.view<TrackHistory, Transform>();
                for (auto&& [entity, track, transform] : view.each())
                {
                    if (transform.position.valid())
                    {
                        if (track.chunks.empty() || track.chunks.back().points.size() >= track_chunk_size)
                        {
                            createNewChunk(track, transform);
                        }

                        updateChunk(track, track.chunks.back(), transform);
                    }
                }

                last_update = now;
            }
        };

        void createNewChunk(TrackHistory& track, Transform& transform)
        {
            // Make a new chunk and set its reference point
            auto& chunk = track.chunks.emplace_back();
            chunk.attach_point = registry.create();
            chunk.referencePoint = transform.position;

            // Each chunk get a line primitive.
            auto& line = registry.emplace<Line>(chunk.attach_point);
            line.style = track.style;
            line.referencePoint = chunk.referencePoint;

            // If this is not the first chunk, connect it to the previous one
            if (track.chunks.size() > 1)
            {
                auto& prev_chunk = track.chunks[track.chunks.size() - 2];
                chunk.points.emplace_back(prev_chunk.points.back());
            }

            // pre-allocates space for all future points
            line.staticSize = track_chunk_size; 

            registry.get<Visibility>(chunk.attach_point).setAll(visible);
        }

        void updateChunk(TrackHistory& track, TrackHistory::Chunk& chunk, Transform& transform)
        {
            if (chunk.points.size() > 0 && chunk.points.back() == to_vsg((glm::dvec3)(transform.position)))
                return;

            // append the new point
            chunk.points.emplace_back(to_vsg((glm::dvec3)transform.position));

            // and copy over the point array to the line primitive.
            auto& line = registry.get<Line>(chunk.attach_point);
            //line.points().emplace_back(chunk.points.back());
            line.points() = chunk.points;
            line.revision++;
        }

        void updateVisibility()
        {
            auto view = registry.view<TrackHistory>();
            for (auto&& [entity, track] : view.each())
            {
                for (auto& chunk : track.chunks)
                    registry.get<Visibility>(chunk.attach_point).setAll(visible);
            }
        }
    };

#if 0
    class TrackHistoryCollector
    {
    public:
        Application& app;
        float update_hertz = 1.0f; // updates per second
        TrackHistorySystem system;

        TrackHistoryCollector(Application& in_app) :
            app(in_app),
            system(in_app.registry) { }

        void run()
        {
            jobs::context context;
            context.pool = jobs::get_pool("rocky.track_history", 1);

            jobs::dispatch([this]()
                {
                    scoped_use use(app.handle);

                    while (app.active())
                    {
                        run_at_frequency f(update_hertz);
                        system.update(app.runtime());
                        app.runtime().requestFrame();
                    }
                    Log()->info("Track history thread terminating.");

                }, context);
        }
    };
#endif
}

auto Demo_TrackHistory = [](Application& app)
{
    static bool init = false;
    //static int max_tracks = 100;
    static std::shared_ptr<TrackHistorySystem> system;

    if (!system)
    {
        app.ecsManager->add(system = TrackHistorySystem::create(app.registry));

        LineStyle style;
        style.color = vsg::vec4{ 0.0f, 1.0f, 1.0f, 1.0f };
        style.width = 2.0f;

        //int count = 0;
        auto view = app.registry.view<Transform>();
        for (auto&& [entity, transform] : view.each())
        {
            auto& track = app.registry.emplace<TrackHistory>(entity);
            track.style = style;
            //if (count++ > max_tracks) break;
        }
    }

    if (ImGuiLTable::Begin("track history"))
    {
        if (ImGuiLTable::Checkbox("Visible", &system->visible))
        {
            system->updateVisibility();
        }
        //ImGuiLTable::Text("Max tracks", "%d", max_tracks);
        ImGuiLTable::SliderFloat("Update frequency", &system->update_hertz, 1.0f, 15.0f);
        ImGuiLTable::End();
    }
};
