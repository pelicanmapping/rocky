/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Image.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    //! Coordinate convention of a texture's image data.
    enum class TextureOrigin
    {
        LowerLeft,
        UpperLeft
    };

    //! How a texture's RGB values are encoded relative to alpha.
    enum class TextureAlphaMode
    {
        Straight,
        Premultiplied
    };

    /**
     * Shareable image texture for geometry and projected content.
     * Use only one texture source component on an entity. Call dirty() after
     * changing image contents in place. Texture coordinates belong to the consumer.
     */
    struct ImageTexture : public Component<ImageTexture>
    {
        Image::Ptr image;
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };
}
