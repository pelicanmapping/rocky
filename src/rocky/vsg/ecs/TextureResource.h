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
    enum class TextureResourceProducer
    {
        External,
        ImageTexture,
        RenderTexture
    };

    /** VSG-side producer/consumer boundary for texture content. */
    struct TextureResource : public Component<TextureResource>
    {
        vsg::ref_ptr<vsg::ImageInfo> texture;
        std::uint64_t revision = 0u;
        TextureResourceProducer producer = TextureResourceProducer::External;
        bool ready = true;
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };
}
