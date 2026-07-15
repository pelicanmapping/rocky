/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ViewDependentState.h>
#include <rocky/Rendering.h>
#include <rocky/Callbacks.h>

namespace ROCKY_NAMESPACE
{
    struct DecalGPU
    {
        glm::fmat4 mvm;
        glm::fmat4 mvmInverse;
        union {
            glm::float32 halfX;
            glm::float32 zMin;
        };
        union {
            glm::float32 halfY;
            glm::float32 zMax;
        };
        union {
            glm::float32 halfZ;
            glm::float32 cullingRadius;
        };
        std::int32_t count = 0; // used by entry 0 as total decal count
        float distance = 0.0f; // > 0 = persp
        float tanHalfFovY = 0.0f;
        float aspect = 1.0f;
        float _padding = 0.0f;
    };
    static_assert(sizeof(DecalGPU) % 16 == 0, "DecalGPU must be 16-byte aligned");

    // Render data that can be shared across systems. 
    // VSGContextImpl creates and owns a unique instance of this.
    struct SharedRenderData
    {
        SharedRenderData();

        ViewLocal<vsg::ref_ptr<ViewDependentStateEx>> viewDependentState;

        vsg::ref_ptr<vsg::DescriptorBuffer> decalsBuf;

        Callback<> onDecalsBufReallocated;
    };
}

