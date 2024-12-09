/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Application.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Rendering = [](Application& app)
{
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

        ImGuiLTable::Checkbox("Render on demand", &app.context->renderOnDemand);

        //ImGuiLTable::Checkbox("Manual rendering", &app.context->manualRendering);
        //if (app.context->manualRendering)
        //{
        //    ImGui::SameLine();
        //    if (ImGui::Button("Render next frame"))
        //        app.context->renderingEnabled = true;
        //}

        auto& c = app.mapNode->terrainNode->stategroup->children;
        bool wireframe = c.front() == setWireframeTopology;

        if (ImGuiLTable::Checkbox("Wireframe", &wireframe))
        {
            auto& c = app.mapNode->terrainNode->stategroup->children;
            if (wireframe)
                c.insert(c.begin(), setWireframeTopology);
            else
                c.erase(c.begin());
        }

        ImGuiLTable::End();
    }
};
