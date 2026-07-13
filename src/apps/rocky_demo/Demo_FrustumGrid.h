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
    float debug = params->debugTiles;

    if (ImGuiLTable::SliderFloat("Show frustum grid", &debug, 0.0f, 1.0f))
    {
        for(ViewIDType viewID = 0; viewID < ROCKY_MAX_NUMBER_OF_VIEWS; ++viewID)
        {
            auto vds = app.vsgcontext->sharedRenderData->viewDependentState[viewID];
            if (vds) {
                BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
                params->debugTiles = std::clamp(debug, 0.0f, 1.0f);
                app.vsgcontext->upload(params);
            }
        }
    }

    ImGuiLTable::End();
};
