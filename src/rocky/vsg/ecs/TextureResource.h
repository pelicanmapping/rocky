/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Texture.h>
#include <vsg/state/ImageInfo.h>

namespace ROCKY_NAMESPACE::detail
{
    //! Identifies which subsystem may replace or remove a TextureResource.
    //! Consumers such as DecalSystem never dispose producer-owned image data.
    enum class TextureResourceProducer
    {
        External,
        Texture,
        ImageTexture,
        RenderTexture
    };

    /**
     * VSG-side producer/consumer boundary for shared sampled images.
     *
     * TextureSystem publishes Texture and ImageTexture; OverlayBakeSystem
     * publishes RenderTexture. MeshSystem and DecalSystem observe revisions
     * and retain descriptor references without taking over producer disposal.
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
        //! Sampling/compositing metadata shared by all consumers.
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };
}
