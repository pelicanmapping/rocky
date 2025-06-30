/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs.h>
#include <rocky/vsg/NodePager.h>
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

namespace
{
    class EntityNode : public vsg::Inherit<vsg::Node, EntityNode>
    {
    public:
        ecs::Registry& r;
        entt::entity entity = entt::null;
        TileKey key;

        EntityNode(ecs::Registry& reg) : r(reg) {}

        void traverse(vsg::RecordTraversal& record) const override
        {
            ROCKY_SOFT_ASSERT(entity != entt::null);
            if (entity != entt::null)
            {
                auto viewID = record.getCommandBuffer()->viewID;

                auto [lock, registry] = r.read();
                auto& v = registry.get<Visibility>(entity);
                v.frame[viewID] = record.getFrameStamp()->frameCount;
            }
        }

        void destroy()
        {
            if (entity != entt::null)
            {
                auto [lock, registry] = r.write();
                registry.destroy(entity);
                entity = entt::null;
            }
        }

        // for debugging.
        bool validate(entt::registry& registry) const
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(entity != entt::null, false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.valid(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<ActiveState>(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<Visibility>(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<Transform>(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<TransformDetail>(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<Line>(entity), false);
            ROCKY_SOFT_ASSERT_AND_RETURN(registry.try_get<Widget>(entity), false);
            return true;
        }
    };
}

auto Demo_NodePager = [](Application& app)
{
    static const vsg::vec4 colors[4] = {
        vsg::vec4{1, 0, 0, 1}, // red
        vsg::vec4{0, 1, 0, 1}, // green
        vsg::vec4{0, 0, 1, 1}, // blue
        vsg::vec4{1, 1, 0, 1}  // yellow
    };

    static vsg::ref_ptr<NodePager> pager;
    static CallbackToken onExpire;

    if (!pager)
    {
        pager = NodePager::create(app.mapNode->profile);

        // Mandatory: the function that will create the payload for each TileKey:
        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                auto node = EntityNode::create(app.registry);
                node->key = key;

                auto [lock, registry] = app.registry.write();

                node->entity = registry.create();

                auto& transform = registry.emplace<Transform>(node->entity);
                transform.position = key.extent().centroid();

                auto& line = registry.emplace<Line>(node->entity);
                auto w = key.extent().width(Units::METERS) * 0.4, h = key.extent().height(Units::METERS) * 0.4;
                line.points = {
                    vsg::dvec3{-w, -h, 0.0},
                    vsg::dvec3{ w, -h, 0.0},
                    vsg::dvec3{ w,  h, 0.0},
                    vsg::dvec3{-w,  h, 0.0},
                    vsg::dvec3{-w, -h, 0.0} };
                line.style = LineStyle{ colors[key.level % 4], 2.0f };

                auto& widget = registry.emplace<Widget>(node->entity);
                widget.text = key.str();

                auto& visibility = registry.get<Visibility>(node->entity);
                visibility.enableFrameAgeVisibility(true);

                return node;
            };
        
        // Optional: a callback to invoke when data is about to be paged out:
        onExpire = pager->onExpire([&app](vsg::ref_ptr<vsg::Object> object)
            {
                util::forEach<EntityNode>(object, [&](EntityNode* node)
                    {
                        node->destroy();
                    });
            });

        // Always initialize a NodePager before using it:
        pager->initialize(app.context);

        app.mainScene->addChild(pager);
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        ImGuiLTable::Text("Tiles", "%u", pager->tiles());
        ImGuiLTable::SliderFloat("Screen Space Error", &pager->screenSpaceError, 32.0f, 512.0f, "%.0f px");

        if (ImGuiLTable::Button("Reload"))
            pager->initialize(app.context);


        ImGuiLTable::End();

        auto keys = pager->tileKeys();
        if (!keys.empty())
        {
            std::sort(keys.begin(), keys.end());
            int count = 0;
            for (auto& key : keys)
            {   
                ImGui::Text(key.str().c_str());
                //if (ImGui::Button(key.str().c_str()))
                //    pager->debugKey = key;
                if (++count % 6 > 0) ImGui::SameLine();
            }
            ImGui::Text("");
        }
    }
};
