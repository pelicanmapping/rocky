/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs.h>
#include <rocky/vsg/NodePager.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_NodePager = [](Application& app)
{
    static const vsg::vec4 colors[4] = {
        vsg::vec4{1, 0, 0, 1}, // red
        vsg::vec4{0, 1, 0, 1}, // green
        vsg::vec4{0, 0, 1, 1}, // blue
        vsg::vec4{1, 1, 0, 1}  // yellow
    };

    static vsg::ref_ptr<NodePager> pager;
    static Profile profile("global-geodetic");

    if (!pager)
    {
        pager = NodePager::create(profile, app.mapNode->profile);

        pager->refinePolicy = NodePager::RefinePolicy::Add;

        // Mandatory: the function that will create the payload for each TileKey:
        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<EntityNode> result;

                // Geometry generator:
                FeatureView fview;

                // Set up a style for lines:
                fview.styles.line = LineStyle();
                fview.styles.line->color = colors[key.level % 4];
                fview.styles.line->width = 2.0f;
                fview.styles.line->depth_offset = 10000; // meters

                // The extents of the tile, transformed into the map SRS:
                auto ex = key.extent().transform(app.mapNode->mapSRS());

                // Add features for the extents of the tile box:
                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymin(), 0),
                        glm::dvec3(ex.xmax(), ex.ymin(), 0)
                    },
                    GeodeticInterpolation::RhumbLine));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymax(), 0),
                        glm::dvec3(ex.xmax(), ex.ymax(), 0)
                    },
                    GeodeticInterpolation::RhumbLine));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmax(), ex.ymin(), 0),
                        glm::dvec3(ex.xmax(), ex.ymax(), 0)
                    }));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymin(), 0),
                        glm::dvec3(ex.xmin(), ex.ymax(), 0)
                    }));


                // Make the entities for this tile:
                auto prims = fview.generate(app.mapNode->worldSRS(), app.vsgcontext);

                if (!prims.empty())
                {
                    // An EntityNode to cull the entities in the scene graph:
                    result = EntityNode::create(app.registry);

                    app.registry.write([&](entt::registry& registry)
                        {
                            result->entities.emplace_back(prims.moveToEntity(registry));

                            // Add a label in the center of the tile:
                            auto entity = registry.create();
                            registry.emplace<Visibility>(entity);
                            result->entities.emplace_back(entity);

                            auto& widget = registry.emplace<Widget>(entity);
                            widget.text = key.str();

                            auto& transform = registry.emplace<Transform>(entity);
                            transform.position = key.extent().centroid().transform(app.mapNode->worldSRS());
                        });
                }

                return result;
            };

        // Always initialize a NodePager before using it:
        pager->initialize(app.vsgcontext);

        app.mainScene->addChild(pager);
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        static std::vector<std::string> pn = { "global-geodetic", "spherical-mercator" };
        int profileIndex = 0;
        for (int i = 0; i < pn.size(); i++)
            if (pn[i] == profile.wellKnownName())
                profileIndex = i;

        if (ImGuiLTable::BeginCombo("Profile", pn[profileIndex].c_str()))
        {
            for (int i = 0; i < pn.size(); ++i)
            {
                if (ImGui::RadioButton(pn[i].c_str(), profileIndex == i))
                {
                    profileIndex = i;
                    profile = Profile(pn[i]);                    
                    app.vsgcontext->onNextUpdate([&]()
                        {
                            util::remove(pager, app.mainScene->children);
                            pager = nullptr;
                        });

                    app.vsgcontext->requestFrame();
                }
            }
            ImGuiLTable::EndCombo();
        }

        ImGuiLTable::Text("Tiles", "%u", pager->tiles());

        bool add = pager->refinePolicy == NodePager::RefinePolicy::Add;
        if (ImGuiLTable::Checkbox("Additive", &add))
        {
            pager->refinePolicy = add ? NodePager::RefinePolicy::Add : NodePager::RefinePolicy::Replace;
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::SliderFloat("Screen Space Error", &pager->screenSpaceError, 64.0f, 1024.0f, "%.0f px"))
        {
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::Button("Reload"))
        {
            pager->initialize(app.vsgcontext);
            app.vsgcontext->requestFrame();
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
