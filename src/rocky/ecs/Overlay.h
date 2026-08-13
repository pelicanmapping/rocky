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
    * Overlay is a control component that will cause a geometry
    * component (like Mesh, Line, etc.) attached to the same entity
    * to drape on the terrain instead of rendering in normal space.
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
