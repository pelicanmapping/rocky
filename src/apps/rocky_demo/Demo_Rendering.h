/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Application.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

namespace
{
    Layer::Ptr createAxesLayer(const Ellipsoid& ell, Registry& registry)
    {
        auto layer = EntityCollectionLayer::create(registry);
        layer->name = "Axes";
        registry.write([&](entt::registry& r)
            {
                const double length = 1e6;

                layer->entities.emplace_back(r.create());
                auto& xline = r.emplace<Line>(layer->entities.back());
                xline.points = { { ell.semiMajorAxis(),0,0 }, { ell.semiMajorAxis() + length, 0, 0} };
                xline.style.color = Color::Lime;

                layer->entities.emplace_back(r.create());
                auto& yline = r.emplace<Line>(layer->entities.back());
                yline.points = { { 0,ell.semiMajorAxis(),0 }, { 0, ell.semiMajorAxis() + length, 0} };
                yline.style.color = Color::Red;

                layer->entities.emplace_back(r.create());
                auto& zline = r.emplace<Line>(layer->entities.back());
                zline.points = { { 0,0,ell.semiMinorAxis() }, { 0, 0, ell.semiMinorAxis() + length} };
                zline.style.color = Color::Cyan;
            });
        return layer;
    }
}

auto Demo_Rendering = [](Application& app)
{
    static Layer::Ptr axesLayer;
    // Better would be vkCmdSetPolygonMode extension, but it is noy supported by VSG
    // This will do in the meantime.
    static vsg::ref_ptr<vsg::SetPrimitiveTopology> setWireframeTopology;        
    if (!setWireframeTopology)
    {
        setWireframeTopology = vsg::SetPrimitiveTopology::create();
        setWireframeTopology->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }

    if (ImGuiLTable::Begin("rendering"))
    {
        ImGuiLTable::SliderFloat("Screen space error", &app.mapNode->terrainSettings().screenSpaceError.mutable_value(), 0.0f, 512.0f, "%.0f");

        ImGuiLTable::Checkbox("Render continuously", &app.vsgcontext->renderContinuously);

        auto& c = app.mapNode->terrainNode->children;
        bool wireframe = c.front() == setWireframeTopology;

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

        int maxLevel = app.mapNode->terrainSettings().maxLevelOfDetail.value();
        if (ImGuiLTable::SliderInt("Max LOD", &maxLevel, 0, 20))
        {
            app.mapNode->terrainSettings().maxLevelOfDetail = maxLevel;
        }

        static bool showAxes = false;
        if (ImGuiLTable::Checkbox("Show axes", &showAxes))
        {
            if (showAxes)
            {
                if (!axesLayer)
                    app.mapNode->map->add(axesLayer = createAxesLayer(app.mapNode->srs().ellipsoid(), app.registry));

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

        ImGuiLTable::End();
    }
};
