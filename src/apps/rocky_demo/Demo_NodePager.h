/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Registry.h>
#include <rocky/vsg/NodePager.h>
#include <rocky/vsg/ecs/EntityNode.h>
#include <rocky/ElevationSampler.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_NodePager = [](Application& app)
{
    static const Color colors[4] = {
        Color{1, 0, 0, 1}, // red
        Color{0, 1, 0, 1}, // green
        Color{0, 0, 1, 1}, // blue
        Color{1, 1, 0, 1}  // yellow
    };

    static vsg::ref_ptr<NodePager> pager;
    static Profile profile("global-geodetic");
    static ElevationSampler clamper;

    if (!pager)
    {
        clamper.layer = app.mapNode->map->layer<ElevationLayer>();

        pager = NodePager::create(profile, app.mapNode->profile);

        pager->minLevel = 3;

        pager->maxLevel = 14;

        pager->refinePolicy = NodePager::RefinePolicy::Replace;

        pager->calculateBound = [&](const TileKey& key, const IOOptions& io)
            {
                auto ex = app.mapNode->profile.clampAndTransformExtent(key.extent());
                auto bs = ex.createWorldBoundingSphere(0, 0);

                if (clamper.ok() && key.level > 1)
                {
                    auto p = ex.centroid();
                    auto session = clamper.session(io);
                    session.lod = std::min(key.level, 5u);
                    session.xform = p.srs.to(clamper.layer->profile.srs());

                    if (clamper.clamp(session, p.x, p.y, p.z))
                    {
                        p.transformInPlace(app.mapNode->worldSRS());
                        return vsg::dsphere(to_vsg(p), bs.radius);
                    }
                }

                return to_vsg(bs);
            };

        // Mandatory: the function that will create the payload for each TileKey:
        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<EntityNode> result;

                auto centroid = key.extent().centroid().transform(SRS::WGS84);

                if (clamper.ok())
                {
                    auto session = clamper.session(io);
                    session.lod = std::min(key.level, 6u);
                    session.xform = centroid.srs.to(clamper.layer->profile.srs());
                    centroid.z = clamper.sample(session, centroid.x, centroid.y, centroid.z);
                }

                // Geometry generator:
                FeatureView fview;

                // Set up a style for lines:
                fview.styles.line.color = colors[key.level % 4];
                fview.styles.line.width = 2.0f;
                fview.styles.line.depth_offset = 10000; // meters

                // The extents of the tile, transformed into the map SRS:
                auto ex = key.extent().transform(app.mapNode->mapSRS());
                ex.expand(-ex.width() * 0.025, -ex.height() * 0.025); // shrink a tad

                // Add features for the extents of the tile box:
                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymin(), centroid.z),
                        glm::dvec3(ex.xmax(), ex.ymin(), centroid.z)
                    },
                    GeodeticInterpolation::RhumbLine));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymax(), centroid.z),
                        glm::dvec3(ex.xmax(), ex.ymax(), centroid.z)
                    },
                    GeodeticInterpolation::RhumbLine));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmax(), ex.ymin(), centroid.z),
                        glm::dvec3(ex.xmax(), ex.ymax(), centroid.z)
                    }));

                fview.features.emplace_back(Feature(
                    ex.srs(),
                    Geometry::Type::LineString, {
                        glm::dvec3(ex.xmin(), ex.ymin(), centroid.z),
                        glm::dvec3(ex.xmin(), ex.ymax(), centroid.z)
                    }));


                // Make the entities for this tile:
                auto prims = fview.generate(app.mapNode->worldSRS());

                if (!prims.empty())
                {
                    // An EntityNode to cull the entities in the scene graph:
                    result = EntityNode::create(app.registry);

                    app.registry.write([&](entt::registry& registry)
                        {
                            result->entities.emplace_back(prims.move(registry));

                            // Add a label in the center of the tile:
                            auto entity = registry.create();
                            registry.emplace<Visibility>(entity);
                            result->entities.emplace_back(entity);

                            auto& widget = registry.emplace<Widget>(entity);
                            widget.text = key.str();

                            auto& transform = registry.emplace<Transform>(entity);
                            transform.position = centroid.transform(app.mapNode->worldSRS());
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
                if (++count % 6 > 0) ImGui::SameLine();
            }
            ImGui::Text(" ");
        }
    }
};
