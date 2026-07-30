/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Image.h>
#include <rocky/ecs/Component.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    struct DecalStyle : public Component<DecalStyle>
    {
        //! Image to use for decal
        Image::Ptr image;

        //! Texture dimensions in world units (meters)
        std::optional<glm::dvec2> textureSize;

        //! Opacity
        float opacity = 1.0f;
    };


    struct Decal : public Component<Decal>
    {
        // testing!
        entt::entity optics = entt::null;

        //! Style entity, if applicable
        entt::entity style = entt::null;

        //! Construct a default decal
        Decal() = default;

        //! Construct a decal with a style attached to a different entity
        Decal(entt::entity styleEntity) : 
            Component<Decal>(), style(styleEntity) {}
    };
}
