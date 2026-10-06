/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs/ECSNode.h>
#include "helpers.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <unordered_map>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! VSG event handler that runs an intersection.
    class DemoIntersectMouseHandler : public vsg::Inherit<vsg::Visitor, DemoIntersectMouseHandler>
    {
    public:
        //! Retains the application used for picking; it must outlive this event handler.
        DemoIntersectMouseHandler(Application& in_app) : app(in_app) {}

        int buffer = 3;
        bool highlightHovered = true;
        bool pulse = false;
        Color hoverColor = Highlight{}.color;
        Callback<std::unordered_set<entt::entity>> onIntersect;

        //! Stores the latest pick result and immediately updates demo-owned hover highlights.
        void highlight(const std::unordered_set<entt::entity>& hits)
        {
            hovered = hits;
            refreshHighlights();
        }

        //! Updates only demo-owned tints under the registry write lock; preserves existing highlights.
        //! Frame events and UI edits call this on the application thread before the next ECS update.
        void refreshHighlights()
        {
            auto color = hoverColor;
            if (pulse)
            {
                const double seconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - pulseEpoch).count();
                // A 1.5-second cycle between 30% and 100% of the chosen tint strength.
                color.a *= static_cast<float>(0.65 + 0.35 * std::cos(seconds * 2.0 * 3.141592653589793 / 1.5));
            }
            bool changed = false;
            app.registry.write([&](entt::registry& reg)
            {
                for (auto it = highlighted.begin(); it != highlighted.end();)
                {
                    const auto entity = it->first;
                    auto* tint = reg.valid(entity) ? reg.try_get<Highlight>(entity) : nullptr;
                    // Relinquish ownership if an application action removed or replaced our last tint.
                    if (!tint || tint->color != it->second)
                    {
                        it = highlighted.erase(it);
                        continue;
                    }
                    if (!highlightHovered || hovered.count(entity) == 0u ||
                        !reg.any_of<Mesh, Line, Point, Polygon>(entity) || reg.any_of<Overlay>(entity))
                    {
                        reg.remove<Highlight>(entity);
                        changed = true;
                        it = highlighted.erase(it);
                        continue;
                    }
                    if (tint->color != color)
                    {
                        tint->color = color;
                        changed = true;
                    }
                    it->second = color;
                    ++it;
                }
                if (highlightHovered)
                {
                    for (auto entity : hovered)
                    {
                        if (reg.valid(entity) && reg.any_of<Mesh, Line, Point, Polygon>(entity) &&
                            !reg.any_of<Overlay, Highlight>(entity))
                        {
                            reg.emplace<Highlight>(entity).color = color;
                            highlighted.emplace(entity, color);
                            changed = true;
                        }
                    }
                }
            });
            const bool animating = pulse && highlightHovered && hoverColor.a > 0.0f && !highlighted.empty();
            if (changed || animating)
                app.vsgcontext->requestFrame();
        }

    protected:
        Application& app;
        std::unordered_set<entt::entity> hovered;
        std::unordered_map<entt::entity, Color> highlighted;
        const std::chrono::steady_clock::time_point pulseEpoch = std::chrono::steady_clock::now();

        //! Advances active hover pulses, including while the pointer is stationary; idle hover requests no frames.
        void apply(vsg::FrameEvent&) override
        {
            if (!highlighted.empty())
                refreshHighlights();
        }

        //! Picks ordinary scene geometry and clears hover when the pointer leaves a view or enters UI.
        void apply(vsg::MoveEvent& e) override
        {
            std::unordered_set<entt::entity> hits;
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                if (auto& window = app.display.find(e.window.ref_ptr()))
                {
                    if (auto& view = window.viewAtCoords((float)e.x, (float)e.y))
                    {
                        auto i = ECSPolytopeIntersector::create(
                            view.vsgView, e.x - buffer, e.y - buffer, e.x + buffer, e.y + buffer);
                        app.scene->accept(*i);
                        hits = std::move(i->collectedEntities);
                    }
                }
            }
            highlight(hits);
            onIntersect.fire(std::move(hits));
        }
    };
}

auto Demo_Intersect = [](Application& app)
{
    static CallbackSubs subs;
    static std::unordered_set<entt::entity> entities;
    static vsg::ref_ptr<DemoIntersectMouseHandler> handler;

    if (subs.empty())
    {
        // install our mouse handler:
        handler = DemoIntersectMouseHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        subs += handler->onIntersect([&](std::unordered_set<entt::entity>&& in_entities)
            {
                entities = std::move(in_entities);
            });
    }

    ImGui::TextWrapped("Create objects in the geometry demos, then hover over them. "
        "Hover stays enabled until unchecked. Models and projected overlays are excluded. "
        "Hover color alpha controls tint strength; Pulse animates that strength.");
    if (ImGuiLTable::Begin("Entity Intersect"))
    {
        bool changed = ImGuiLTable::Checkbox("Highlight hovered entities", &handler->highlightHovered);
        changed |= ImGuiLTable::Checkbox("Pulse", &handler->pulse);
        changed |= ImGuiLTable::ColorEdit4("Hover color", &handler->hoverColor[0]);
        if (changed)
            handler->refreshHighlights();
        ImGuiLTable::SliderInt("Buffer", &handler->buffer, 0, 20);
        ImGuiLTable::Text("Found:", "%u", entities.size());
        
        app.registry.read([&](entt::registry& reg)
            {
                for (auto e : entities)
                {
                    ImGui::Separator();
                    std::string types;

                    if (reg.try_get<Widget>(e)) types += "Widget ";
                    if (reg.try_get<Label>(e)) types += "Label ";
                    if (reg.try_get<NodeGraph>(e)) types += "NodeGraph ";
                    if (reg.try_get<Mesh>(e)) types += "Mesh ";
                    if (reg.try_get<Line>(e)) types += "Line ";
                    if (reg.try_get<Point>(e)) types += "Point ";
                    if (reg.try_get<Polygon>(e)) types += "Polygon ";

                    ImGuiLTable::TextUnformatted(std::to_string((std::uint32_t)e).c_str(), types.c_str());
                }
            });

        ImGuiLTable::End();
    }
};
