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
    private:
        ecs::Registry _registry;

    public:
        entt::entity entity = entt::null;

        EntityNode(ecs::Registry& reg) :
            _registry(reg)
        {
            auto r = _registry.write();
            entity = r->create();
        }

        virtual ~EntityNode()
        {
            if (entity != entt::null)
            {
                _registry.write()->destroy(entity);
            }
        }

        void traverse(vsg::RecordTraversal& record) const override
        {
            ROCKY_SOFT_ASSERT(entity != entt::null);
            if (entity != entt::null)
            {
                auto viewID = record.getCommandBuffer()->viewID;

                auto [lock, registry] = _registry.write();
                auto& v = registry.get<Visibility>(entity);
                v.frame[viewID] = record.getFrameStamp()->frameCount;
            }
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
    static Profile profile("global-geodetic");

    if (!pager)
    {
        pager = NodePager::create(profile, app.mapNode->profile);

        // Mandatory: the function that will create the payload for each TileKey:
        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                auto node = EntityNode::create(app.registry);

                auto [lock, registry] = app.registry.write();

                auto& transform = registry.emplace<Transform>(node->entity);
                transform.position = key.extent().centroid();
                transform.topocentric = false;

                auto& line = registry.emplace<Line>(node->entity);
                auto w = key.extent().width(Units::METERS) * 0.4, h = key.extent().height(Units::METERS) * 0.4;

                auto keySRS = key.extent().srs();
                auto worldSRS = app.mapNode->worldSRS();
                auto center = to_vsg(key.extent().centroid().transform(worldSRS));
                line.points.resize(5);
                line.points[0] = to_vsg(GeoPoint(keySRS, key.extent().xmin(), key.extent().ymin()).transform(worldSRS)) - center;
                line.points[1] = to_vsg(GeoPoint(keySRS, key.extent().xmax(), key.extent().ymin()).transform(worldSRS)) - center;
                line.points[2] = to_vsg(GeoPoint(keySRS, key.extent().xmax(), key.extent().ymax()).transform(worldSRS)) - center;
                line.points[3] = to_vsg(GeoPoint(keySRS, key.extent().xmin(), key.extent().ymax()).transform(worldSRS)) - center;
                line.points[4] = line.points[0]; // close the loop

                line.style.color = colors[key.level % 4];
                line.style.width = 2.0f;
                line.style.depth_offset = 10000; // meters

                auto& widget = registry.emplace<Widget>(node->entity);
                widget.text = key.str();

                auto& visibility = registry.get<Visibility>(node->entity);
                visibility.enableFrameAgeVisibility(true);

                return node;
            };

        // Always initialize a NodePager before using it:
        pager->initialize(app.context);

        app.mainScene->addChild(pager);
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        static std::string profileNames[2] = { "global-geodetic", "spherical-mercator" };
        static int profileIndex = 0;

        if (ImGuiLTable::BeginCombo("Profile", profileNames[profileIndex].c_str()))
        {
            for (int i = 0; i < 2; ++i)
            {
                if (ImGui::RadioButton(profileNames[i].c_str(), profileIndex == i))
                {
                    profileIndex = i;
                    profile = Profile(profileNames[i]);
                    
                    app.context->onNextUpdate([&]()
                        {
                            util::remove(pager, app.mainScene->children);
                            pager = nullptr;
                        });

                    app.context->requestFrame();
                }
            }
            ImGuiLTable::EndCombo();
        }

        ImGuiLTable::Text("Tiles", "%u", pager->tiles());
        ImGuiLTable::SliderFloat("Screen Space Error", &pager->screenSpaceError, 64.0f, 1024.0f, "%.0f px");

        if (ImGuiLTable::Button("Reload"))
        {
            pager->initialize(app.context);
        }

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
