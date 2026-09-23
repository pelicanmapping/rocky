/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Constrains an entity's Transform altitude to loaded terrain, preserving its horizontal location.
     * While present, the terrain-anchor system owns the Transform altitude;
     * removing the component leaves the last resolved position in place.
     */
    struct TerrainAnchor
    {
        //! Vertical offset above the terrain surface, in meters.
        double offset = 0.0;
    };
}
