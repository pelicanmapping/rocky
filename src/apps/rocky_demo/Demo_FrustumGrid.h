/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Application.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_FrustumGrid = [](Application& app)
{
    ImGuiLTable::Begin("frustum_grid_demo");

    auto vds = app.vsgcontext->sharedRenderData->viewDependentState[0];
    BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);

    if (ImGuiLTable::SliderFloat("Blend tiles", &params->debugTiles, 0.0f, 1.0f))
    {
        app.vsgcontext->upload(params);
    }

    ImGuiLTable::End();
};
