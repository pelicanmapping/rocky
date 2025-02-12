/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Image.h>
#include <rocky/vsg/ecs/Component.h>
#include <optional>

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

    /**
    * Icon Component - an icon is a 2D billboard with a texture
    * at a geolocation.
    */
    struct Icon : public RevisionedComponent
    {
        //! Dynamic styling for the icon
        IconStyle style;

        //! Image to use for the icon texture
        std::shared_ptr<Image> image;
    };

    /**
    * Icon Component - an icon is a 2D billboard with a texture.
    */
    struct Icon2 : public RevisionedComponent
    {
        float size_pixels = 256.0f;
        float rotation_radians = 0.0f;
        std::shared_ptr<Image> image;
    };
}
