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
    const int track_chunk_size = 8;

    // ECS Component
    struct TrackHistory
    {
        struct Chunk
        {
            entt::entity attach_point = entt::null; // attachment point for the line
            std::size_t numPoints = 0;
        };

        entt::entity style = entt::null;
        unsigned maxPoints = 75;

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
                    reg.on_destroy<TrackHistory>().connect<&TrackHistorySystem::on_destroy_TrackHistory>(this);

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

            if (elapsed >= freq)
            {
                auto [lock, reg] = _registry.write();

                auto view = reg.view<TrackHistory, Transform>();
                for (auto&& [entity, track, transform] : view.each())
                {
                    if (transform.position.valid())
                    {
                        if (track.chunks.empty() || track.chunks.back().numPoints >= track_chunk_size)
                        {
                            auto& track = reg.get<TrackHistory>(entity);
                            auto& transform = reg.get<Transform>(entity);
                            addChunk(reg, entity, track, transform);
                            updateChunk(reg, entity, track, transform, track.chunks.back());
                        }
                        else
                        {
                            updateChunk(reg, entity, track, transform, track.chunks.back());

                            // approximation for now.
                            auto maxChunks = track.maxPoints / track_chunk_size;
                            if (track.chunks.size() > 1u && track.chunks.size() > maxChunks)
                            {
                                releaseChunk(reg, std::move(track.chunks.front()));
                                track.chunks.pop_front();
                            }
                        }
                    }
                }

                last_update = now;
            }

            _registry.read([&](entt::registry& registry)
                {
                    updateVisibility(registry);
                });
        };

        void addChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform)
        {
            // toggle back and forth between 2 styles
            auto style = trackStyles[track.chunks.size() % 2];

            // generate a new chunk
            auto chunk = createChunk(registry, style);            
            track.chunks.emplace_back(chunk);

            // and tie its visibility to that of the host entity (the platform itself)
            updateVisibility(registry, host_entity, chunk);
            
            auto& geom = registry.get<LineGeometry>(chunk.attach_point);
            geom.srs = transform.position.srs;

            // If this is not the first chunk, connect it to the previous one
            if (track.chunks.size() > 1)
            {
                auto prev_chunk = std::prev(std::prev(track.chunks.end()));
                auto& prev_geom = registry.get<LineGeometry>(prev_chunk->attach_point);

                geom.points.emplace_back(prev_geom.points.back());
                geom.dirty(registry);
                chunk.numPoints++;
            }          
            
            // We don't know whether the new chunk is brand new, or
            // came from a freelist, so just ensure it is active:
            (void) registry.get_or_emplace<ActiveState>(chunk.attach_point);
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
            registry.view<TrackHistory>().each([&](auto entity, auto& track)
                {
                    for (auto& chunk : track.chunks)
                    {
                        updateVisibility(registry, entity, chunk);
                    }
                });
        }

        void reset()
        {
            _registry.write([&](entt::registry& reg)
                {
                    // first delete any existing track histories
                    reg.clear<TrackHistory>();

                    // then re-scan and add new ones.
                    reg.view<Transform>().each([&](auto entity, auto& transform)
                        {
                            auto& track = reg.emplace<TrackHistory>(entity);
                            track.style = trackStyles[0];
                        });
                });
        }

    protected:

        std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

        // called by EnTT when a TrackHistory component is destroyed.
        void on_destroy_TrackHistory(entt::registry& registry, entt::entity entity)
        {
            auto& track = registry.get<TrackHistory>(entity);
            for (auto& chunk : track.chunks)
            {
                releaseChunk(registry, std::move(chunk));
            }
            track.chunks.clear();
        }

        void releaseChunk(entt::registry& reg, TrackHistory::Chunk&& chunk)
        {
#if 0
            // uncomment this to completely destroy the chunk instead of recycling it
            reg.destroy(chunk.attach_point);
#else
            reg.remove<ActiveState>(chunk.attach_point);
            auto& geom = reg.get<LineGeometry>(chunk.attach_point);            
            geom.recycle(reg);
            freelist.emplace_back(std::move(chunk));
#endif
        }

        TrackHistory::Chunk createChunk(entt::registry& reg, entt::entity styleEntity)
        {
            if (freelist.empty())
            {
                // creates a brand new chunk and associates a new Line/LineGeometry with it;
                // the style is passed in and shared.
                TrackHistory::Chunk chunk;
                chunk.attach_point = reg.create();
                auto& geom = reg.emplace<LineGeometry>(chunk.attach_point);
                auto& style = reg.get<LineStyle>(styleEntity);
                reg.emplace<Line>(chunk.attach_point, geom, style);
                return chunk;
            }

            else
            {
                auto chunk = std::move(freelist.back());
                freelist.pop_back();

                auto& geom = reg.get<LineGeometry>(chunk.attach_point);
                geom.points.clear();
                chunk.numPoints = 0;
                geom.dirty(reg);

                auto& line = reg.get<Line>(chunk.attach_point);
                if (line.style != styleEntity)
                {
                    line.style = styleEntity;
                    line.dirty(reg);
                }

                return chunk;
            }
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
