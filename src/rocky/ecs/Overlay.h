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
    //! Rendering pathway used for an overlay decal.
    enum class OverlayTechnique
    {
        RTT,  //!< Rasterize the overlay into a render-to-texture image.
#ifdef ROCKY_HAS_SLUGHORN
        //! Experimental, vector-only analytic decal pathway. This supports a
        //! deliberately restricted subset of Rocky geometry and style semantics.
        Slug  //!< Encode vectors in a Slughorn atlas and evaluate them in the decal shader.
#endif
    };

    /**
    * Control component that projects same-entity geometry on terrain as a decal.
    */
    struct Overlay : public Component<Overlay>
    {
        //! Decal rendering pathway. RTT preserves the established behavior.
        OverlayTechnique technique = OverlayTechnique::RTT;

        //! RTT dimensions in pixels. Slug uses this as the nominal scale for
        //! screen-unit line widths and point sizes; physical line widths are
        //! independent of this value.
        glm::uvec2 textureSize = { 512u, 512u };

        //! Modulation color.
        Color color = StockColor::White;

        //! Whether to use depth testing when rendering an RTT overlay. Leave this
        //! disabled for flat artwork that should composite in draw order; enable
        //! it for 3D or nonplanar geometry that requires self-occlusion.
        bool useDepthBuffer = false;

        //! Re-render an RTT overlay texture every frame. Leave this disabled for
        //! static geometry; enable it for animated models or other content that
        //! changes without dirtying its ECS components.
        bool continuousBake = false;
    };
}
