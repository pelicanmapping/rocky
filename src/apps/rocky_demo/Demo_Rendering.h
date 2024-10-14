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
    if (ImGuiLTable::Begin("rendering"))
    {
        ImGuiLTable::SliderFloat("Screen space error", &app.mapNode->terrainSettings().screenSpaceError.mutable_value(), 0.0f, 512.0f, "%.0f");

        //ImGuiLTable::SliderFloat("Tile pixels", &app.mapNode->terrainSettings().tilePixelSize.mutable_value(), 1.0f, 512.0f, "%.0f");

        ImGuiLTable::Checkbox("Render on demand", &app.instance.renderOnDemand());

        ImGuiLTable::End();
    }
};
