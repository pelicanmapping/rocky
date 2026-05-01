/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Application.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

namespace
{
    Layer::Ptr createAxesLayer(Application& app, const Ellipsoid& ell)
    {
        const float len = 2.0 * (ell.semiMajorAxis() + 1e6);
        const float width = 25'000;

        auto group = vsg::Group::create();

        vsg::Builder builder;
        vsg::StateInfo si;
        si.lighting = false;

        vsg::GeometryInfo gi;
        gi.dx = { width, 0, 0 }, gi.dy = { 0, width, 0 }, gi.dz = { 0, 0, len*0.95f };

        gi.color = to_vsg(StockColor::Cyan);
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(StockColor::Lime);
        gi.transform = vsg::rotate(vsg::dquat({ 0,0,1 }, { 1,0,0 }));
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(StockColor::Red);
        gi.transform = vsg::rotate(vsg::dquat({ 0,0,1 }, { 0,1,0 }));
        group->addChild(builder.createCylinder(gi, si));

        auto enode = EntityNode::create(app.registry);
        group->addChild(enode);

        app.registry.write([&](entt::registry& r)
            {
                auto& style = r.emplace<LabelStyle>(r.create());

                auto add = [&](const GeoPoint& pos, const std::string& text)
                    {
                        auto entity = enode->entities.emplace_back(r.create());
                        r.emplace<Transform>(entity).position = pos;
                        r.emplace<Label>(entity, text, style);
                    };

                add(GeoPoint(SRS::ECEF, 0, 0, (len / 2)), "+Z");
                add(GeoPoint(SRS::ECEF, (len / 2), 0, 0), "+X");
                add(GeoPoint(SRS::ECEF, 0, (len / 2), 0), "+Y");
                add(GeoPoint(SRS::ECEF, 0, 0, -(len / 2)), "-Z");
                add(GeoPoint(SRS::ECEF, -(len / 2), 0, 0), "-X");
                add(GeoPoint(SRS::ECEF, 0, -(len / 2), 0), "-Y");
            });

        auto layer = NodeLayer::create();
        layer->name = "Axes";
        layer->node = group;

        app.vsgcontext->compile(group);

        return layer;
    }
}

auto Demo_Terrain = [](Application& app)
{
    static Layer::Ptr axesLayer;

    auto&& [window, view] = app.display.windowAndViewAtCoords(ImGui::GetMousePos().x, ImGui::GetMousePos().y);

    if (!window || !view) return;

    auto mapNode = view.find<MapNode>();
    if (!mapNode) return;

    if (ImGuiLTable::Begin("terrain"))
    {
        ImGuiLTable::SliderFloat("Pixel error", &app.mapNode->terrainSettings().pixelError.mutable_value(), 0.0f, 512.0f, "%.0f");

        if (app.vsgcontext->device()->getPhysicalDevice()->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
        {
            ImGuiLTable::Checkbox("Triangles", &mapNode->terrainNode->debugTriangles.mutable_value());
        }

        ImGuiLTable::Checkbox("Lighting", &mapNode->terrainSettings().lighting.mutable_value());

        if (auto sky = view.find<SkyNode>())
        {
            if (sky->sun->shadowSettings)
            {
                ImGuiLTable::Checkbox("Cast shadows", &mapNode->terrainSettings().castShadows.mutable_value());
            }
        }

        ImGuiLTable::Checkbox("Normals", &mapNode->terrainSettings().debugNormals.mutable_value());
        ImGuiLTable::Checkbox("Wireframe", &mapNode->terrainNode->wireframe.mutable_value());

        bool skirts = mapNode->terrainSettings().skirtRatio.value() > 0.0f;
        if (ImGuiLTable::Checkbox("Tile skirts", &skirts))
        {
            if (skirts)
                mapNode->terrainSettings().skirtRatio = 0.05f;
            else
                mapNode->terrainSettings().skirtRatio = 0.0f;

            mapNode->terrainNode->reset(app.vsgcontext);
        }

        static bool showAxes = false;
        if (ImGuiLTable::Checkbox("Show axes", &showAxes))
        {
            if (showAxes)
            {
                if (!axesLayer)
                    mapNode->map->add(axesLayer = createAxesLayer(app, mapNode->srs().ellipsoid()));

                auto r = axesLayer->open(app.io());
                if (r.failed())
                    Log()->info("Failed to open axes layer: " + r.error().message);
            }
            else if (axesLayer)
            {
                axesLayer->close();
            }

            app.vsgcontext->requestFrame();
        }

        int maxLevel = mapNode->terrainSettings().maxLevel.value();
        if (ImGuiLTable::SliderInt("Max level", &maxLevel, 0, 23))
        {
            mapNode->terrainSettings().maxLevel = maxLevel;
        }

        ImGuiLTable::SliderInt("L2 cache size", (int*)&mapNode->terrainSettings().tileCacheSize.mutable_value(), 0, 4096);

        float* nodata = (float*)&mapNode->terrainSettings().backgroundColor.mutable_value();
        ImGuiLTable::ColorEdit3("No-data color", nodata);

        ImGuiLTable::End();
    }
};
