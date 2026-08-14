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
    /**
    * Convenience component that renders same-entity geometry to a texture and
    * projects it on terrain. Internally this adapts to RenderTexture,
    * ProjectedTexture, and RenderParticipation. Use those lower-level
    * components directly when sources, render job, and projector are separate.
    */
    struct Overlay : public Component<Overlay>
    {
        //! Overlay texture dimensions in pixels.
        glm::uvec2 textureSize = { 512u, 512u };

        //! Modulation color.
        Color color = StockColor::White;

        //! Whether the offscreen bake should use depth testing. Leave this
        //! disabled for flat artwork that should composite in draw order; enable
        //! it for 3D or nonplanar geometry that requires self-occlusion.
        bool useDepthBuffer = false;

        //! Re-render the overlay texture every frame. Leave this disabled for
        //! static geometry; enable it for animated models or other content that
        //! changes without dirtying its ECS components.
        bool continuousBake = false;
    };
}
