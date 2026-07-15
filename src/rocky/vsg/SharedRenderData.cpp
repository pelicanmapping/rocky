/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "SharedRenderData.h"
#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;

SharedRenderData::SharedRenderData()
{
    BufferAccess<DecalGPU> decals(
        decalsBuf,
        BINDING_DECALS, TYPE_DECALS);
}
