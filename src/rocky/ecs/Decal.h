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
    enum class DecalProjection
    {
        Orthographic,
        Perspective            
    };

    struct DecalStyle : public Component<DecalStyle>
    {
        //! Image to use for decal
        Image::Ptr image;

        //! Texture dimensions in world units (meters)
        std::optional<glm::dvec2> textureSize;
    };

    struct Decal : public Component<Decal>
    {
        //! Projection mode
        DecalProjection projection = DecalProjection::Orthographic;

        //! Vertical field of view in degrees [perspective projection]
        float fovY_deg = 45.0f;

        //! Width / height ratio [perspective projection]
        float aspectRatio = 1.0f;

        //! Distance from projector to target in meters [perspective projection].
        float distance = 100.0f;

        //! Style entity, if applicable
        entt::entity style = entt::null;


        Decal() = default;
        Decal(entt::entity styleEntity) : 
            Component<Decal>(), style(styleEntity) {}
    };
}
