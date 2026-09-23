/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Color.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    //! Rendering mode used to draw an overlay on terrain.
    enum class OverlayMode
    { 
        //! Render content at a specified image resolution
        Raster,

        //! Resolution-independent rendering for vector geometry.
        //! Unsupported styling is omitted with a warning; supported content still renders in Vector mode.
        //! Falls back to Raster with a log message if vector rendering is unavailable
        //! or geometry complexity exceeds its capacity, using Overlay::resolution.
        Vector
    };

    /**
     * Draws geometry on this entity as an overlay on the terrain.
     */
    struct Overlay : public Component<Overlay>
    {
        //! Requested rendering mode for this overlay. Defaults to Vector.
        OverlayMode mode = OverlayMode::Vector;

        //! Image resolution (X,Y) for raster rendering, in pixels.
        glm::uvec2 resolution = { 512u, 512u };

        //! Modulation color.
        Color color = StockColor::White;

        //! Whether to use depth testing when rendering a raster overlay. Leave this
        //! disabled for flat artwork that should composite in draw order; enable
        //! it for 3D or nonplanar geometry that requires self-occlusion.
        bool useDepthBuffer = false;

        //! Re-render a raster overlay every frame. Leave this disabled for
        //! static geometry; enable it for animated models or other content that
        //! changes without dirtying its ECS components.
        bool continuousBake = false;
    };
}
