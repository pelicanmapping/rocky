/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Component that works with a Transform to dynamic constrain the draw size
    * of an object to a range of pixel sizes on screen.
    */
    struct PixelScale
    {
        bool enabled = true;
        float minPixels = 16.0f;
        float maxPixels = 4096.0f;
    };
}
