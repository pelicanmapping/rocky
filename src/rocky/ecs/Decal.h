/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Image.h>
#include <rocky/Color.h>
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

        //! Color to modulate with the texture image (if there is one)
        Color color = StockColor::White;
    };


    struct Decal : public Component<Decal>
    {
        //! Optional entity containing the Optics used to project this decal. When
        //! unset, Optics on this decal's own entity is used when present; otherwise
        //! the decal is an orthographic projection of its transformed unit cube.
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
