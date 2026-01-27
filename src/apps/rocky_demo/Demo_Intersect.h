/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs/ECSNode.h>
#include "helpers.h"
#include <filesystem>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! VSG event handler that runs an intersection.
    class DemoIntersectMouseHandler : public vsg::Inherit<vsg::Visitor, DemoIntersectMouseHandler>
    {
    public:
        DemoIntersectMouseHandler(Application& in_app) : app(in_app) {}

        int buffer = 3;
        Callback<void(std::vector<entt::entity>)> onIntersect;

    protected:
        Application& app;

        void apply(vsg::MoveEvent& e) override
        {
            auto view = app.display.viewAtWindowCoords(e.window, e.x, e.y);
            if (view)
            {
                auto i = ECSPolytopeIntersector::create(view, e.x - buffer, e.y - buffer, e.x + buffer, e.y + buffer);
                app.mainScene->accept(*i);
                onIntersect.fire(i->collectedEntities);
            }
        }
    };
}

auto Demo_Intersect = [](Application& app)
{
    static CallbackSubs subs;
    static std::vector<entt::entity> entities;
    static vsg::ref_ptr<DemoIntersectMouseHandler> handler;

    if (subs.empty())
    {
        // install our mouse handler:
        handler = DemoIntersectMouseHandler::create(app);
        app.viewer->getEventHandlers().emplace_back(handler);

        subs += handler->onIntersect([&](std::vector<entt::entity>&& in_entities)
            {
                entities = std::move(in_entities);
            });
    }

    if (ImGuiLTable::Begin("Entity Intersect"))
    {
        ImGuiLTable::SliderInt("Buffer", &handler->buffer, 0, 20);
        ImGuiLTable::Text("Found:", "%u", entities.size());
        
        app.registry.read([&](entt::registry& reg)
            {
                for (auto e : entities)
                {
                    ImGui::Separator();
                    auto* mesh = reg.try_get<Mesh>(e);
                    if (mesh) ImGuiLTable::Text("Mesh", "entity %u", e);
                    auto* line = reg.try_get<Line>(e);
                    if (line) ImGuiLTable::Text("Line", "entity %u", e);
                    auto* node = reg.try_get<NodeGraph>(e);
                    if (node) ImGuiLTable::Text("NodeGraph", "entity %u", e);
                }
            });

        ImGuiLTable::End();
    }
};
