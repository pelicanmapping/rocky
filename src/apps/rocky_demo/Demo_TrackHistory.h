/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/System.h>
#include "helpers.h"
#include <chrono>
#include <limits>
#include <deque>

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
            entt::entity attach_point = entt::null; // attachment point for the line
            std::size_t numPoints = 0;
        };

        entt::entity style = entt::null;
        unsigned maxPoints = 48;

    private:
        std::deque<Chunk> chunks;
        friend class TrackHistorySystem;
    };

    class TrackHistorySystem : public System
    {
    public:
        float update_hertz = 1.0f; // updates per second
        bool tracks_visible = true; // global visibility for all tracks
        entt::entity trackStyles[2]; // style for tracks
        std::vector<TrackHistory::Chunk> freelist; // for recycling used chunks

    public:
        //! Construct a new system for managing TrackHistory components.
        static auto create(Registry& r) { 
            return std::make_shared<TrackHistorySystem>(r);
        }

        //! Construct a new system for managing TrackHistory components.
        //! Please call create(registry)
        TrackHistorySystem(Registry& r) : System(r)
        {
            r.write([&](entt::registry& reg)
                {
                    // destruction of a TrackHistory requries some extra work:
                    reg.on_destroy<TrackHistory>().connect<&TrackHistorySystem::on_destroy>(this);

                    // default track style
                    trackStyles[0] = reg.create();
                    auto& style1 = reg.emplace<LineStyle>(trackStyles[0]);
                    style1.color = Color::Lime;
                    style1.width = 2.0f;

                    trackStyles[1] = reg.create();
                    auto& style2 = reg.emplace<LineStyle>(trackStyles[1]);
                    style2.color = Color::Red;
                    style2.width = 2.0f;
                });
        }

        void update(VSGContext& vsgcontext) override
        {
            auto now = std::chrono::steady_clock::now();
            auto freq = 1s / update_hertz;
            auto elapsed = (now - last_update);
            std::vector<entt::entity> chunks_to_add;
            std::vector<entt::entity> chunks_to_deactivate;

            if (elapsed >= freq)
            {
                auto [lock, registry] = _registry.write();

                auto view = registry.view<TrackHistory, Transform>();
                for (auto&& [entity, track, transform] : view.each())
                {
                    if (transform.position.valid())
                    {
                        if (track.chunks.empty() || track.chunks.back().numPoints >= track_chunk_size)
                        {
                            chunks_to_add.emplace_back(entity);
                        }
                        else
                        {
                            updateChunk(registry, entity, track, transform, track.chunks.back());

                            // approximation for now.
                            auto maxChunks = track.maxPoints / track_chunk_size;
                            if (track.chunks.size() > 1u && track.chunks.size() > maxChunks)
                            {
                                chunks_to_deactivate.emplace_back(track.chunks.front().attach_point);
                                add_to_freelist(registry, std::move(track.chunks.front()));
                                track.chunks.pop_front();
                            }
                        }
                    }
                }

                last_update = now;
            }

            if (!chunks_to_add.empty())
            {
                _registry.write([&](entt::registry& reg)
                    {
                        for (auto entity : chunks_to_add)
                        {
                            auto& track = reg.get<TrackHistory>(entity);
                            auto& transform = reg.get<Transform>(entity);
                            createNewChunk(reg, entity, track, transform);
                            updateChunk(reg, entity, track, transform, track.chunks.back());
                        }
                    });
            }

            if (!chunks_to_deactivate.empty())
            {
                _registry.write([&](entt::registry& reg)
                    {
                        for (auto entity : chunks_to_deactivate)
                        {
                            reg.remove<ActiveState>(entity);
                        }
                    });
            }

            _registry.read([&](entt::registry& registry)
                {
                    updateVisibility(registry);
                });
        };

        void createNewChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform)
        {
            auto style = trackStyles[track.chunks.size() % 2];

            // Check the freelist first
            if (!freelist.empty())
            {
                auto& c = track.chunks.emplace_back(std::move(take_from_freelist()));

                auto& geom = registry.get<LineGeometry>(c.attach_point);
                geom.points.clear(), c.numPoints = 0;
                geom.dirty(registry);

                auto& line = registry.get<Line>(c.attach_point);
                if (line.style != style)
                {
                    line.style = style;
                    line.dirty(registry);
                }
            }
            else
            {
                auto& c = track.chunks.emplace_back();
                c.attach_point = registry.create();

                // Each chunk get a line primitive.
                auto& geom = registry.emplace<LineGeometry>(c.attach_point);
                geom.points.reserve(track_chunk_size);

                registry.emplace<Line>(c.attach_point, geom, registry.get<LineStyle>(style));

                // Tie track visibility to host visibility:
                updateVisibility(registry, host_entity, c);
            }
            
            // Make a new chunk and set its reference point
            auto& new_chunk = track.chunks.back();

            auto& geom = registry.get<LineGeometry>(new_chunk.attach_point);
            geom.srs = transform.position.srs;

            // If this is not the first chunk, connect it to the previous one
            if (track.chunks.size() > 1)
            {
                auto prev_chunk = std::prev(std::prev(track.chunks.end()));
                auto& prev_geom = registry.get<LineGeometry>(prev_chunk->attach_point);

                auto& geom = registry.get<LineGeometry>(new_chunk.attach_point);
                geom.points.emplace_back(prev_geom.points.back());
                geom.dirty(registry);
                new_chunk.numPoints++;
            }          
            
            // activate (if necessary).
            registry.emplace_or_replace<ActiveState>(new_chunk.attach_point);
        }

        void updateChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform, TrackHistory::Chunk& chunk)
        {
            auto& geom = registry.get<LineGeometry>(chunk.attach_point);

            if (geom.points.size() > 0 && geom.points.back() == (glm::dvec3)(transform.position))
                return;

            // append the new position:
            geom.points.emplace_back((glm::dvec3)transform.position);
            geom.dirty(registry);
            chunk.numPoints++;
        }

        void updateVisibility(entt::registry& registry, entt::entity host_entity, TrackHistory::Chunk& chunk)
        {
            auto& track_visibility = registry.get<Visibility>(chunk.attach_point);
            if (tracks_visible && registry.all_of<ActiveState>(host_entity))
            {
                // ensure that the visibility of a track matches the visibility of its host.
                track_visibility.visible = registry.get<Visibility>(host_entity).visible;
            }
            else
            {
                track_visibility.visible.fill(false);
            }
        }

        void updateVisibility(entt::registry& registry)
        {
            for (auto&& [host_entity, track] : registry.view<TrackHistory>().each())
            {
                for (auto& chunk : track.chunks)
                {
                    updateVisibility(registry, host_entity, chunk);
                }
            }
        }

        void reset()
        {
            _registry.write([&](entt::registry& reg)
                {
                    // first delete any existing track histories
                    reg.clear<TrackHistory>();

                    // then re-scan and add new ones.
                    auto view = reg.view<Transform>();
                    for (auto&& [entity, transform] : view.each())
                    {
                        auto& track = reg.emplace<TrackHistory>(entity);
                        track.style = trackStyles[0];
                    }
                });
        }

    protected:

        std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

        // called by EnTT when a TrackHistory component is destroyed.
        void on_destroy(entt::registry& registry, entt::entity entity)
        {
            auto& track = registry.get<TrackHistory>(entity);
            for (auto& chunk : track.chunks)
            {
                registry.remove<ActiveState>(chunk.attach_point);
                add_to_freelist(registry, std::move(chunk));
            }
            track.chunks.clear();
        }

        void add_to_freelist(entt::registry& reg, TrackHistory::Chunk&& chunk)
        {
            // prep the graphic for possible recycling:
            auto& geom = reg.get<LineGeometry>(chunk.attach_point);
            
            geom.recycle(reg);

            freelist.emplace_back(std::move(chunk));
        }

        TrackHistory::Chunk take_from_freelist()
        {
            auto chunk = std::move(freelist.back());
            freelist.pop_back();
            return chunk;
        }
    };
}


auto Demo_TrackHistory = [](Application& app)
{
    static bool init = false;
    static std::shared_ptr<TrackHistorySystem> system;

    if (!system)
    {
        // make a System to handle track histories, and add it to the app.
        system = TrackHistorySystem::create(app.registry);
        app.ecsNode->add(system);
        system->reset();
    }

    if (ImGuiLTable::Begin("track history"))
    {
        ImGuiLTable::Checkbox("Show", &system->tracks_visible);

        ImGuiLTable::SliderFloat("Update frequency", &system->update_hertz, 1.0f, 15.0f);

        app.registry.read([&](entt::registry& r)
            {
                auto& style = r.get<LineStyle>(system->trackStyles[0]);
                if (ImGuiLTable::ColorEdit3("Color", &style.color.x))
                    style.dirty(r);

                if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 5.0f))
                    style.dirty(r);
            });
        
        ImGuiLTable::Text("Freelist size", "%ld", system->freelist.size());

        ImGuiLTable::End();
    }

    ImGui::Separator();
    if (ImGui::Button("Reset"))
    {
        system->reset();
    }
};
