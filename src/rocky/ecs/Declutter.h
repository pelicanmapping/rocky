/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS component whose precense on an entity indicates that the entity
    * should particpate in decluttering.
    */
    struct Declutter
    {
        float priority = 0.0f;
        Rect rect;
    };
}
