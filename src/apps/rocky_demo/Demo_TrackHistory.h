/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs.h>
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

        LineStyle style;
        unsigned maxPoints = 48;

    private:
        std::deque<Chunk> chunks;
        friend class TrackHistorySystem;
    };

    class TrackHistorySystem : public ecs::System
    {
    public:
        //! Construct a new system for managing TrackHistory components.
        static auto create(ecs::Registry& r) { 
            return std::make_shared<TrackHistorySystem>(r);
        }

        //! Construct a new system for managing TrackHistory components.
        //! Please call create(registry)
        TrackHistorySystem(ecs::Registry& r) : ecs::System(r)
        {
            auto [lock, registry] = r.write();

            // destruction of a TrackHistory requries some extra work:
            registry.on_destroy<TrackHistory>().connect<&TrackHistorySystem::on_destroy>(this);

            // default track style
            style.color = vsg::vec4{ 0.0f, 1.0f, 0.0f, 1.0f };
            style.width = 2.0f;
        }

        float update_hertz = 1.0f; // updates per second
        bool tracks_visible = true; // global visibility for all tracks
        LineStyle style; // style for tracks
        std::vector<TrackHistory::Chunk> freelist; // for recycling used chunks

        void update(VSGContext& runtime) override
        {
            auto now = std::chrono::steady_clock::now();
            auto freq = 1s / update_hertz;
            auto elapsed = (now - last_update);
            std::vector<entt::entity> chunks_to_add;
            std::vector<entt::entity> chunks_to_deactivate;

            if (elapsed >= freq)
            {
                auto [lock, registry] = _registry.read();

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
                auto [lock, registry] = _registry.write();

                for (auto entity : chunks_to_add)
                {
                    auto& track = registry.get<TrackHistory>(entity);
                    auto& transform = registry.get<Transform>(entity);
                    createNewChunk(registry, entity, track, transform);
                    updateChunk(registry, entity, track, transform, track.chunks.back());
                }
            }

            if (!chunks_to_deactivate.empty())
            {
                auto [lock, registry] = _registry.write();

                for (auto entity : chunks_to_deactivate)
                {
                    registry.remove<ActiveState>(entity);
                }
            }
        };

        void createNewChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform)
        {
            // Check the freelist first
            if (!freelist.empty())
            {
                auto& c = track.chunks.emplace_back(std::move(take_from_freelist()));
                Line& line = registry.get<Line>(c.attach_point);
                line.points.clear(), c.numPoints = 0;
                line.dirty();
            }
            else
            {
                auto& c = track.chunks.emplace_back();
                c.attach_point = registry.create();

                // Each chunk get a line primitive.
                auto& line = registry.emplace<Line>(c.attach_point);
                line.style = track.style;
                line.points.reserve(track_chunk_size);

                // pre-allocates space for all future points
                line.staticSize = track_chunk_size;

                // Tie track visibility to host visibility:
                updateVisibility(registry, host_entity, c);
            }
            
            // Make a new chunk and set its reference point
            auto& new_chunk = track.chunks.back();

            Line& line = registry.get<Line>(new_chunk.attach_point);
            line.referencePoint = transform.position;

            // If this is not the first chunk, connect it to the previous one
            if (track.chunks.size() > 1)
            {
                auto prev_chunk = std::prev(std::prev(track.chunks.end()));
                auto& prev_line = registry.get<Line>(prev_chunk->attach_point);

                Line& line = registry.get<Line>(new_chunk.attach_point);
                line.points.emplace_back(prev_line.points.back());
                line.dirtyPoints();
                new_chunk.numPoints++;
            }          
            
            // activate (if necessary).
            registry.emplace_or_replace<ActiveState>(new_chunk.attach_point);
        }

        void updateChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform, TrackHistory::Chunk& chunk)
        {
            auto& line = registry.get<Line>(chunk.attach_point);

            if (line.points.size() > 0 && line.points.back() == to_vsg((glm::dvec3)(transform.position)))
                return;

            // append the new position:
            line.points.emplace_back(to_vsg((glm::dvec3)transform.position));
            line.dirtyPoints();
            chunk.numPoints++;
        }

        void updateVisibility(entt::registry& registry, entt::entity host_entity, TrackHistory::Chunk& chunk)
        {
            auto& track_visibility = registry.get<Visibility>(chunk.attach_point);
            if (tracks_visible)
            {
                track_visibility.parent = &registry.get<Visibility>(host_entity);
            }
            else
            {
                track_visibility.parent = nullptr;
                track_visibility.fill(false);
            }
        }

        void updateVisibility(entt::registry& registry)
        {
            // ensure that the visibility of a track matches the visibility of its host.
            auto view = registry.view<TrackHistory>();
            for (auto&& [host_entity, track] : view.each())
            {
                for (auto& chunk : track.chunks)
                {
                    updateVisibility(registry, host_entity, chunk);
                }
            }
        }

        void updateStyle(entt::registry& registry)
        {
            auto view = registry.view<TrackHistory>();
            for (auto&& [entity, track] : view.each())
            {
                track.style = style;
                for (auto& chunk : track.chunks)
                {
                    auto& line = registry.get<Line>(chunk.attach_point);
                    line.style = track.style;
                    line.dirtyStyle();
                }
            }
        }

        void reset()
        {
            auto [lock, registry] = _registry.write();

            // first delete any existing track histories
            registry.clear<TrackHistory>();

            // then re-scan and add new ones.
            auto view = registry.view<Transform>();
            for (auto&& [entity, transform] : view.each())
            {
                //if (transform.parent == nullptr)
                {
                    auto& track = registry.emplace<TrackHistory>(entity);
                    track.style = style;
                }
            }
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

        void add_to_freelist(entt::registry& registry, TrackHistory::Chunk&& chunk)
        {
            // prep the graphic for possible recycling:
            auto& line = registry.get<Line>(chunk.attach_point);
            line.recycle(registry);

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
        app.ecsManager->add(system);
        system->reset();
    }

    if (ImGuiLTable::Begin("track history"))
    {
        if (ImGuiLTable::Checkbox("Show", &system->tracks_visible))
        {
            auto [lock, registry] = app.registry.read();
            system->updateVisibility(registry);
        }

        ImGuiLTable::SliderFloat("Update frequency", &system->update_hertz, 1.0f, 15.0f);

        if (ImGuiLTable::ColorEdit3("Color", system->style.color.data()))
        {
            auto [lock, registry] = app.registry.read();
            system->updateStyle(registry);
        }

        if (ImGuiLTable::SliderFloat("Width", &system->style.width, 1.0f, 5.0f))
        {
            auto [lock, registry] = app.registry.read();
            system->updateStyle(registry);
        }
        
        ImGuiLTable::Text("Freelist size", "%ld", system->freelist.size());

        ImGuiLTable::End();
    }

    ImGui::Separator();
    if (ImGui::Button("Reset"))
    {
        system->reset();
    }
};
