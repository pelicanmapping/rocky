/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/GDALFeatureSource.h>
#include <rocky/ElevationSampler.h>
#include <rocky/ecs/Registry.h>
#include <rocky/vsg/NodePager.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_MVTFeatures = [](Application& app)
{
#ifdef ROCKY_HAS_GDAL

    static vsg::ref_ptr<NodePager> pager;
    static ElevationSampler clamper;

    if (!pager)
    {
        // Set up our elevation clamper.
        clamper.layer = app.mapNode->map->layer<ElevationLayer>();

        pager = NodePager::create(Profile("spherical-mercator"), app.mapNode->profile);

        pager->minLevel = 14;
        pager->maxLevel = 14;
        pager->refinePolicy = NodePager::RefinePolicy::Add;

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

        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<vsg::Node> result;

                // Feature source that will read MVT from the intercloud:
                auto gdal = GDALFeatureSource::create();
                gdal->uri = "MVT:https://readymap.org/readymap/mvt/osm/" + key.str() + ".pbf";
                gdal->openOptions.emplace_back("CLIP=NO");

                auto status = gdal->open();
                if (status.failed())
                {
                    Log()->warn(status.error().message);
                    return result;
                }

                FeatureView fview;

                // specify an origin to localize our geometry:
                fview.origin = key.extent().centroid();

                fview.styles.line.color = Color::Red;
                fview.styles.line.width = 5.0f;
                fview.styles.line.depth_offset = 10; // meters

                fview.styles.mesh.color = Color(1, 0.75f, 0.2f, 1);
                fview.styles.mesh.depth_offset = 10; // meters

                if (gdal->featureCount() > 0)
                    fview.features.reserve(gdal->featureCount());

                // iterate over all the features and pick the ones we want
                gdal->each(io, [&](Feature&& f)
                    {
                        if (f.hasField("building") && f.geometry.type == Geometry::Type::Polygon)
                        {
                            fview.features.emplace_back(std::move(f));
                        }

                        else if (f.field("highway") == "motorway" ||
                            f.field("highway") == "trunk" ||
                            f.field("highway") == "primary" ||
                            f.field("highway") == "secondary" ||
                            f.field("highway") == "tertiary")
                        {
                            // convert to a line string:
                            fview.features.emplace_back(std::move(f));
                        }
                    });
                
                if (!fview.features.empty())
                {
                    if (clamper.ok())
                    {
                        // configure a sampling session since we're doing a batch of work:
                        auto session = clamper.session(io);
                        session.lod = key.level;

                        // transform points to the proper SRS:
                        session.xform = fview.features.front().srs.to(clamper.layer->profile.srs());

                        for (auto& f : fview.features)
                        {
                            f.geometry.eachPart([&](Geometry& part)
                                {
                                    // clamp the geometry to the elevation layer:
                                    auto r = clamper.clampRange(session, part.points.begin(), part.points.end());
                                    if (r.failed())
                                        return;
                                });
                        }
                    }

                    // generate primitives from features:
                    auto prims = fview.generate(app.mapNode->worldSRS());

                    if (!prims.empty())
                    {
                        auto node = EntityNode::create(app.registry);

                        // Take a write-lock to move the primitives into ECS entities.
                        app.registry.write([&](entt::registry& registry)
                            {
                                auto e = prims.move(registry);

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
        pager->initialize(app.vsgcontext);

        // Add it as a layer :)
        auto layer = NodeLayer::create(pager);
        layer->name = "MVT Features Demo Layer";
        if (layer->open(app.io()).ok())
            app.mapNode->map->add(layer);
    }

    if (ImGuiLTable::Begin("NodePager"))
    {
        if (ImGuiLTable::SliderFloat("Screen Space Error", &pager->screenSpaceError, 64.0f, 1024.0f, "%.0f px"))
        {
            app.vsgcontext->requestFrame();
        }

        ImGuiLTable::End();

        auto window = app.viewer->windows().front();
        auto view = app.display.getView(window, 0, 0);
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
#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "Unavailable - not built with GDAL");
#endif
};
