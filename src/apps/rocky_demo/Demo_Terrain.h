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
    Layer::Ptr createAxesLayer(Application& app, const Ellipsoid& ell, VSGContext vsg)
    {
        const float len = 2.0 * (ell.semiMajorAxis() + 1e6);
        const float width = 25'000;

        auto group = vsg::Group::create();

        vsg::Builder builder;
        vsg::StateInfo si;
        si.lighting = false;

        vsg::GeometryInfo gi;
        gi.dx = { width, 0, 0 }, gi.dy = { 0, width, 0 }, gi.dz = { 0, 0, len*0.95f };

        gi.color = to_vsg(Color::Cyan);
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(Color::Lime);
        gi.transform = vsg::rotate(vsg::dquat({ 0,0,1 }, { 1,0,0 }));
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(Color::Red);
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

        vsg->compile(group);

        return layer;
    }
}

auto Demo_Terrain = [](Application& app)
{
    static Layer::Ptr axesLayer;
    // Better would be vkCmdSetPolygonMode extension, but it is not supported by VSG
    // This will do in the meantime.
    static vsg::ref_ptr<vsg::SetPrimitiveTopology> setWireframeTopology;        
    if (!setWireframeTopology)
    {
        setWireframeTopology = vsg::SetPrimitiveTopology::create();
        setWireframeTopology->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }

    if (ImGuiLTable::Begin("terrain"))
    {
        ImGuiLTable::SliderFloat("Pixel error", &app.mapNode->terrainSettings().pixelError.mutable_value(), 0.0f, 512.0f, "%.0f");

        auto& c = app.mapNode->terrainNode->children;
        bool wireframe = c.front() == setWireframeTopology;

        if (app.vsgcontext->device()->getPhysicalDevice()->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
        {
            ImGuiLTable::Checkbox("Triangles", &app.mapNode->terrainNode->wireOverlay.mutable_value());
        }

        if (ImGuiLTable::Checkbox("Wireframe", &wireframe))
        {
            if (wireframe)
                c.insert(c.begin(), setWireframeTopology);
            else
                c.erase(c.begin());
        }

        ImGuiLTable::Checkbox("Lighting", &app.mapNode->terrainSettings().lighting.mutable_value());

        bool skirts = app.mapNode->terrainSettings().skirtRatio.value() > 0.0f;
        if (ImGuiLTable::Checkbox("Tile skirts", &skirts))
        {
            if (skirts)
                app.mapNode->terrainSettings().skirtRatio = 0.025f;
            else
                app.mapNode->terrainSettings().skirtRatio = 0.0f;

            app.mapNode->terrainNode->reset(app.vsgcontext);
        }

        static bool showAxes = false;
        if (ImGuiLTable::Checkbox("Show axes", &showAxes))
        {
            if (showAxes)
            {
                if (!axesLayer)
                    app.mapNode->map->add(axesLayer = createAxesLayer(app, app.mapNode->srs().ellipsoid(), app.vsgcontext));

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

        int maxLevel = app.mapNode->terrainSettings().maxLevel.value();
        if (ImGuiLTable::SliderInt("Max level", &maxLevel, 0, 23))
        {
            app.mapNode->terrainSettings().maxLevel = maxLevel;
        }

        ImGuiLTable::SliderInt("L2 cache size", (int*)&app.mapNode->terrainSettings().tileCacheSize.mutable_value(), 0, 4096);

        float* bg = (float*)&app.mapNode->terrainSettings().backgroundColor.mutable_value();
        ImGuiLTable::ColorEdit3("Background color", bg);

        static std::vector<std::string> options = { "global-geodetic", "global-qsc", "spherical-mercator" };
        int index = util::indexOf(options, app.mapNode->profile.wellKnownName());
        if (index >= 0)
        {
            if (ImGuiLTable::BeginCombo("Rendering profile", options[index].c_str()))
            {
                for (int i = 0; i < options.size(); ++i)
                {
                    if (ImGui::RadioButton(options[i].c_str(), index == i))
                    {
                        app.mapNode->profile = Profile(options[i]);
                        if (auto view = app.display.viewAtWindowCoords(app.viewer->windows().front(), 0, 0))
                            if (auto manip = MapManipulator::get(view))
                                manip->home();
                        app.vsgcontext->requestFrame();
                    }
                }
                ImGuiLTable::EndCombo();
            }
        }

        ImGuiLTable::End();
    }
};
