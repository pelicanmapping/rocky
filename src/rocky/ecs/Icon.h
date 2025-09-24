/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Image.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Dynamic render settings for an icon.
     */
    struct IconStyle
    {
        float size_pixels = 256.0f;
        float rotation_radians = 0.0f;
        float padding[2];
    };

    static_assert(sizeof(IconStyle) % 16 == 0, "IconStyle must be 16-byte aligned");

    /**
    * Icon Component - an icon is a 2D billboard with a texture
    */
    struct Icon : public AttachableComponent
    {
        //! Dynamic styling for the icon
        IconStyle style;

        //! Image to use for the icon texture
        std::shared_ptr<Image> image;
    };
}
