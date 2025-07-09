/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/GDALFeatureSource.h>
#include <rocky/vsg/NodePager.h>
#include <rocky/vsg/ecs.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_MVTFeatures = [](Application& app)
{
    static vsg::ref_ptr<NodePager> pager;

    if (!pager)
    {
        pager = NodePager::create(Profile("spherical-mercator"), app.mapNode->profile);

        pager->minLevel = 14;
        pager->maxLevel = 14;
        pager->refinePolicy = NodePager::RefinePolicy::Add;

        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<vsg::Node> result;

                // Feature source that will read MVT from the intercloud:
                auto gdal = GDALFeatureSource::create();
                gdal->uri = "MVT:https://readymap.org/readymap/mvt/osm/" + key.str() + ".pbf";

                auto status = gdal->open();
                if (status.failed())
                {
                    Log()->warn(status.message);
                    return result;
                }

                FeatureView fview;

                // specify an origin to localize our geometry:
                fview.origin = key.extent().centroid();

                fview.styles.line = LineStyle();
                fview.styles.line->color = vsg::vec4(0, 1, 0, 1);
                fview.styles.line->width = 5.0f;
                fview.styles.line->depth_offset = 1000; // meters

                fview.styles.mesh = MeshStyle();
                fview.styles.mesh->color = vsg::vec4(1, 0.75f, 0.2f, 1);
                fview.styles.mesh->depth_offset = 1000; // meters

                if (gdal->featureCount() > 0)
                    fview.features.reserve(gdal->featureCount());

                // iterate over all the features and pick the ones we want
                gdal->each(io, [&](Feature&& f)
                    {
                        if (f.hasField("building") && f.geometry.type == Geometry::Type::Polygon)
                        {
                            fview.features.emplace_back(std::move(f));
                        }
                    });
                
                if (!fview.features.empty())
                {
                    // generate primitives from features:
                    auto prims = fview.generate(app.mapNode->worldSRS(), app.context);

                    if (!prims.line.points.empty() || !prims.mesh.triangles.empty())
                    {
                        auto node = EntityNode::create(app.registry);

                        // Take a write-lock to move the primitives into ECS entities.
                        app.registry.write([&](entt::registry& registry)
                            {
                                auto e = registry.create();

                                if (!prims.line.points.empty())
                                    registry.emplace<Line>(e, std::move(prims.line));

                                if (!prims.mesh.triangles.empty())
                                    registry.emplace<Mesh>(e, std::move(prims.mesh));

                                // Since we localized to an origin, the tile needs a transform:
                                auto& xform = registry.get_or_emplace<Transform>(e);
                                xform.position = fview.origin;
                                xform.frustumCulled = false; // NodePager will take care of frustum culling for us

                                node->entities.emplace_back(e);
                            });

                        result = node;
                    }
                }

                return result;
            };

        // Always initialize a NodePager before using it:
        pager->initialize(app.context);

        app.mainScene->addChild(pager);
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        if (ImGuiLTable::SliderFloat("Screen Space Error", &pager->screenSpaceError, 64.0f, 1024.0f, "%.0f px"))
        {
            app.context->requestFrame();
        }

        ImGuiLTable::End();

        auto window = app.viewer->windows().front();
        auto view = app.displayManager->getView(window, 0, 0);
        auto manip = MapManipulator::get(view);

        if (manip)
        {
            if (ImGuiLTable::Button("Zoom 1"))
            {
                Viewpoint vp;
                vp.name = "Helsinki";
                vp.point = GeoPoint(SRS::WGS84, 24.919, 60.162);
                vp.range = Distance(8.0, Units::KILOMETERS);
                manip->setViewpoint(vp);
            }

            if (ImGuiLTable::Button("Zoom 2"))
            {
                Viewpoint vp;
                vp.name = "Tokyo";
                vp.point = GeoPoint(SRS::WGS84, 139.743, 35.684);
                vp.range = Distance(13.5, Units::KILOMETERS);
                manip->setViewpoint(vp);
            }
        }
    }
};
