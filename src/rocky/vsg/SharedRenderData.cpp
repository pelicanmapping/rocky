/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "SharedRenderData.h"
#include "ShaderDefines.h"
#include "VSGUtils.h"

using namespace ROCKY_NAMESPACE;

SharedRenderData::SharedRenderData()
{
    //nop
}

void
SharedRenderData::initialize(vsg::Device* device)
{
    if (!viewDependentState[0]->frustumsBuf)
    {
        GPUOnlyBufferAccess<FrustumGPU> buf(
            viewDependentState[0]->frustumsBuf,
            BINDING_VDS_FRUSTUMS, TYPE_VDS_FRUSTUMS,
            device);
    }
}
