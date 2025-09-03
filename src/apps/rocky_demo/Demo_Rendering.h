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
    Layer::Ptr createAxesLayer(const Ellipsoid& ell, VSGContext vsg)
    {
        const float len = 2.0 * (ell.semiMajorAxis() + 1e6);
        const float width = 25'000;

        auto group = vsg::Group::create();

        vsg::Builder builder;
        vsg::StateInfo si;
        si.lighting = false;

        vsg::GeometryInfo gi;
        gi.dx = { width, 0, 0 }, gi.dy = { 0, width, 0 }, gi.dz = { 0, 0, len };

        gi.color = to_vsg(Color::Cyan);
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(Color::Lime);
        gi.transform = vsg::rotate(vsg::dquat({ 0,0,1 }, { 1,0,0 }));
        group->addChild(builder.createCylinder(gi, si));

        gi.color = to_vsg(Color::Red);
        gi.transform = vsg::rotate(vsg::dquat({ 0,0,1 }, { 0,1,0 }));
        group->addChild(builder.createCylinder(gi, si));

        auto layer = NodeLayer::create();
        layer->name = "Axes";
        layer->node = group;

        vsg->compile(group);

        return layer;
    }
}

auto Demo_Rendering = [](Application& app)
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

    if (ImGuiLTable::Begin("rendering"))
    {
        ImGuiLTable::SliderFloat("Pixel error", &app.mapNode->terrainSettings().pixelError.mutable_value(), 0.0f, 512.0f, "%.0f");

        ImGuiLTable::Checkbox("Render continuously", &app.vsgcontext->renderContinuously);

        auto& c = app.mapNode->terrainNode->children;
        bool wireframe = c.front() == setWireframeTopology;

        if (app.vsgcontext->device()->getPhysicalDevice()->supportsDeviceExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME))
        {
            ImGuiLTable::Checkbox("Show triangles", &app.mapNode->terrainNode->wireOverlay.mutable_value());
        }

        if (ImGuiLTable::Checkbox("Wireframe", &wireframe))
        {
            if (wireframe)
                c.insert(c.begin(), setWireframeTopology);
            else
                c.erase(c.begin());
        }

        bool skirts = app.mapNode->terrainSettings().skirtRatio.has_value();
        if (ImGuiLTable::Checkbox("Terrain skirts", &skirts))
        {
            if (skirts)
                app.mapNode->terrainSettings().skirtRatio = 0.025f;
            else
                app.mapNode->terrainSettings().skirtRatio.clear();

            app.mapNode->terrainNode->reset(app.vsgcontext);
        }

        int maxLevel = app.mapNode->terrainSettings().maxLevel.value();
        if (ImGuiLTable::SliderInt("Max LOD", &maxLevel, 0, 20))
        {
            app.mapNode->terrainSettings().maxLevel = maxLevel;
        }

        static bool showAxes = false;
        if (ImGuiLTable::Checkbox("Show axes", &showAxes))
        {
            if (showAxes)
            {
                if (!axesLayer)
                    app.mapNode->map->add(axesLayer = createAxesLayer(app.mapNode->srs().ellipsoid(), app.vsgcontext));

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
                        if (auto view = app.display.getView(app.viewer->windows().front(), 0, 0))
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
