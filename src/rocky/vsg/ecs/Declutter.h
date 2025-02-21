/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

namespace ROCKY_NAMESPACE
{
    /**
    * ECS component whose precense on an entity indicates that the entity
    * should particpate in decluttering.
    */
    struct Declutter
    {
        float priority = 0.0f;
        int width_px = -1;
        int height_px = -1;
        int buffer_x = -1;
        int buffer_y = -1;
    };
}
