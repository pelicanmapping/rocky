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
        //! Matrix that places and orients the decal
        glm::dmat4 matrix;

        //! Rendered size of the decal in world units (meters) [orthographic mode]
        glm::dvec3 size = { 100.0, 100.0, 100.0 };

        //! Opacity (alpha)
        float opacity = 1.0f;

        //! Projection mode
        DecalProjection projection = DecalProjection::Orthographic;

        //! Vertical field of view in degrees [perspective projection]
        float fovY_deg = 45.0f;

        //! Width / height ratio [perspective projection]
        float aspectRatio = 1.0f;

        //! Distance from projector to target in meters [perspective projection].
        //! Near/far clips are derived from distance and size.z() (depth):
        //! near = distance - depth/2, far = distance + depth/2.
        float distance = 100.0f;
    };
}
