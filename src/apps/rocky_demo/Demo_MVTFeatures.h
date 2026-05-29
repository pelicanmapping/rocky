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
    static ElevationSampler elevationSampler;
    static entt::entity styleEntity = entt::null;

    if (!pager)
    {
        // An entity to hold our shared styles
        app.registry.write([&](entt::registry& reg)
            {
                styleEntity = reg.create();

                auto& lineStyle = reg.emplace<LineStyle>(styleEntity);
                lineStyle.color = StockColor::Red;
                lineStyle.width = 5.0f;
                lineStyle.depthOffset = 10; // meters

                auto& meshStyle = reg.emplace<MeshStyle>(styleEntity);
                meshStyle.color = Color(1, 0.75f, 0.2f, 1);
                meshStyle.depthOffset = 12; // meters
            });

        // Set up our elevation clamper.
        elevationSampler.layer = app.mapNode->map->layer<ElevationLayer>();

        // Configure a pager that will display paged tiles in mercator profile
        // at LOD 14 only:
        pager = NodePager::create(Profile("spherical-mercator"), app.mapNode->srs());

        pager->minLevel = 14;
        pager->maxLevel = 14;
        pager->refinePolicy = NodePager::RefinePolicy::Replace;
        pager->pixelError = 256;

        // This functor will calculate the bounding sphere for each streamed tile.
        // Since the feature data will be clamped, we need the tile itself to conform
        // to our elevation data set.
        pager->calculateBound = [&](const TileKey& key, const IOOptions& io)
            {
                auto ex = key.extent().transform(app.mapNode->srs());
                auto bs = ex.createWorldBoundingSphere(0, 0);

                if (elevationSampler.ok() && key.level > 1)
                {
                    auto resolutionX = elevationSampler.layer->resolution(key.level).first;
                    if (auto p = elevationSampler.clamp(ex.centroid(), resolutionX, io))
                    {
                        auto n = glm::normalize(bs.center);
                        bs.center += n * p.value().transform(ex.srs().geodeticSRS()).z;
                        bs.center *= 1.01;
                    }
                }

                return bs;
            };

        // This (required) functor will create the actual geometry for each tile
        pager->createPayload = [&app](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<EntityNode> entityNode;

                // Feature source that will read MVT from the intercloud:
                auto gdal = GDALFeatureSource::create();
                gdal->uri = "MVT:https://readymap.org/readymap/mvt/osm/" + key.str() + ".pbf";
                gdal->openOptions.emplace_back("CLIP=NO");

                // Open the feature source and make sure it worked.
                auto status = gdal->open();
                if (status.failed())
                {
                    Log()->warn(status.error().message);
                    return NodePager::Payload::Ptr{};
                }

                std::vector<Feature> buildings, roads;

                // iterate over all the features and pick the ones we want
                gdal->each(io, [&](Feature&& f)
                    {
                        if (f.hasField("building") && f.geometry.type == Geometry::Type::Polygon)
                        {
                            buildings.emplace_back(std::move(f));
                        }

                        else if (f.field("highway") == "motorway" ||
                            f.field("highway") == "trunk" ||
                            f.field("highway") == "primary" ||
                            f.field("highway") == "secondary" ||
                            f.field("highway") == "tertiary")
                        {
                            roads.emplace_back(std::move(f));
                        }
                    });

                // Feature building utility
                FeatureBuilder builder;

                // to clamp features to the terrain
                builder.clamper = elevationSampler.session(io);
                builder.clamper.level = key.level;

                if (!buildings.empty())
                {
                    if (!entityNode)
                        entityNode = EntityNode::create(app.registry); 

                    // copy the style so we don't need a write lock during build:
                    MeshStyle style = app.registry.read().registry.get<MeshStyle>(styleEntity);
                    MeshGeometry geomTemp;
                    builder.buildMeshGeometry(buildings, style, geomTemp);

                    app.registry.write([&](entt::registry& reg)
                        {
                            auto entity = reg.create();
                            auto& geom = reg.emplace<MeshGeometry>(entity, geomTemp);
                            auto& style = reg.get<MeshStyle>(styleEntity);
                            reg.emplace<Mesh>(entity, geom, style);

                            entityNode->entities.emplace_back(entity);
                        });
                }

                if (!roads.empty())
                {
                    if (!entityNode)
                        entityNode = EntityNode::create(app.registry);

                    // copy the style so we don't need a write lock during build:
                    LineStyle style = app.registry.read().registry.get<LineStyle>(styleEntity);
                    LineGeometry geomTemp;
                    builder.buildLineGeometry(roads, style, geomTemp);

                    app.registry.write([&](entt::registry& reg)
                        {
                            auto entity = reg.create();
                            auto& geom = reg.emplace<LineGeometry>(entity, geomTemp);
                            auto& style = reg.get<LineStyle>(styleEntity);
                            reg.emplace<Line>(entity, geom, style);

                            entityNode->entities.emplace_back(entity);
                        });
                }

                return NodePager::Payload::create(entityNode);
            };

        // Always initialize a NodePager before using it:
        pager->initialize(app.vsgcontext);

        // Add it to the map as a layer. This is optional. You can also just add the pager
        // to the scene graph anywhere you want. As a map layer you'll be able to toggle it
        // on and off.
        auto layer = NodeLayer::create(pager);
        layer->name = "MVT Features Demo Layer";
        if (layer->open(app.io()).ok())
            app.mapNode->map->add(layer);

        app.vsgcontext->requestFrame();
    }


    ImGui::TextWrapped("%s", "Mapbox Vector Tiles (MVT) is a spec for streaming tiled vector data.");

    if (ImGuiLTable::Begin("MVTFeatures"))
    {
        if (ImGuiLTable::SliderFloat("Screen Space Error", &pager->pixelError, 64.0f, 1024.0f, "%.0f px"))
        {
            app.vsgcontext->requestFrame();
        }

        ImGuiLTable::End();
}

    auto& view = app.display.window(0).view(0);
    if (auto manip = MapManipulator::get(view.vsgView))
    {
        if (ImGui::Button("Helsinki"))
        {
            Viewpoint vp;
            vp.name = "Helsinki";
            vp.point = GeoPoint(SRS::WGS84, 24.919, 60.162);
            vp.range = Distance(8.0, Units::KILOMETERS);
            manip->setViewpoint(vp);
        }

        ImGui::SameLine();
        if (ImGui::Button("Tokyo"))
        {
            Viewpoint vp;
            vp.name = "Tokyo";
            vp.point = GeoPoint(SRS::WGS84, 139.743, 35.684);
            vp.range = Distance(13.5, Units::KILOMETERS);
            manip->setViewpoint(vp);
        }

        ImGui::SameLine();
        if (ImGui::Button("Quito"))
        {
            Viewpoint vp;
            vp.name = "Quito";
            vp.point = GeoPoint(SRS::WGS84, -78.511, -0.206);
            vp.range = Distance(8.0, Units::KILOMETERS);
            manip->setViewpoint(vp);
        }
    }
#else
    ImGui::TextColored(ImVec4(1, .3, .3, 1), "%s", "Unavailable - not built with GDAL");
#endif
};
