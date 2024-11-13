/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

namespace ROCKY_NAMESPACE
{
    struct Declutter
    {
        float priority = 0.0f;
        int width_px = -1;
        int height_px = -1;
        int buffer_x = -1;
        int buffer_y = -1;
    };
}
