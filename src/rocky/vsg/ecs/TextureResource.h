/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/ProjectedTexture.h>
#include <vsg/state/ImageInfo.h>

namespace ROCKY_NAMESPACE
{
    //! Identifies which subsystem may replace or remove a TextureResource.
    //! Consumers such as DecalSystem never dispose producer-owned image data.
    enum class TextureResourceProducer
    {
        External,
        ImageTexture,
        RenderTexture
    };

    /**
     * VSG-side producer/consumer boundary for projected raster content.
     *
     * TextureSystem populates this from ImageTexture; OverlayBakeSystem
     * populates it from RenderTexture. DecalSystem observes identity/revision,
     * assigns visible resources to descriptor slots, and retains no ownership
     * beyond its descriptor reference.
     */
    struct TextureResource : public Component<TextureResource>
    {
        //! Produced image and sampler. Null means there is no sampled image;
        //! consumers may still apply their documented fallback behavior.
        vsg::ref_ptr<vsg::ImageInfo> texture;
        //! Changes whenever the producer publishes a different result.
        std::uint64_t revision = 0u;
        TextureResourceProducer producer = TextureResourceProducer::External;
        //! Whether consumers may publish this resource for drawing.
        bool ready = true;
        //! Sampling/compositing metadata carried into DecalGPU flags.
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };
}
