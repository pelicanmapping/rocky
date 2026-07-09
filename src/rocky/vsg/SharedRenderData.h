/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/ViewDependentState.h>
#include <rocky/Rendering.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{    
    // Render data that can be shared across systems. 
    // VSGContextImpl creates and owns a unique instance of this.
    struct SharedRenderData
    {
        SharedRenderData();
        void initialize(vsg::Device*);
        ViewLocal<vsg::ref_ptr<ViewDependentStateEx>> viewDependentState;
    };
}

