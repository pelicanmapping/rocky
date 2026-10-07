/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Color.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Visual tint on the entity owning a Polygon, Mesh, Line, or Point.
     * RGB is the tint color; alpha is its blend strength (0 = unchanged, 1 = solid tint).
     * Source opacity, geometry, and shared styles remain unchanged. Models are unsupported.
     * Hover/selection state belongs to the application, which sets or removes this component.
     * Modify under Registry::write; renderers observe changes on update without dirty().
     * Request a frame after editing when rendering on demand.
     * Raster overlays rebake on changes; vector overlays update without rebuilding their atlas.
     * Picking projected overlays is not provided by this component.
     */
    struct Highlight
    {
        Color color = Color(1.0f, 1.0f, 0.0f, 0.65f);
    };
}
