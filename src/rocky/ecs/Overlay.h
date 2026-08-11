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
    };
}
