/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainContext.h"
#include <rocky/Notify.h>

#include <vsg/state/ShaderStage.h>

using namespace rocky;


TerrainContext::TerrainContext(RuntimeContext& rt, const Config& conf) :
    TerrainSettings(conf),
    runtime(rt)
{
    // nop
}
