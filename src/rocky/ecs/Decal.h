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
#include <rocky/ecs/ProjectedTexture.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    //! Image and appearance settings for a Decal.
    struct DecalStyle : public Component<DecalStyle>
    {
        //! Image to use for decal
        Image::Ptr image;

        //! Optional orthographic projection dimensions in world units. These
        //! override the X/Y scale from the projector Transform or Optics pose.
        std::optional<glm::dvec2> textureSize;

        //! Color to modulate with the texture image (if there is one)
        Color color = StockColor::White;
    };

    /**
     * Projects an image onto terrain from an entity's Transform or Optics.
     *
     * A null style or optics reference resolves to this Decal's own entity.
     * Without Optics, the transformed unit cube defines an orthographic
     * projection volume.
     */
    struct Decal : public Component<Decal>
    {
        //! Optional entity containing the Optics used to project this decal. When
        //! unset, Optics on this decal's own entity is used when present; otherwise
        //! the decal is an orthographic projection of its transformed unit cube.
        entt::entity optics = entt::null;

        //! Entity containing the DecalStyle. Null uses this Decal's entity.
        entt::entity style = entt::null;

        //! Decals align their projection with terrain by default. Select Fixed
        //! for a manually positioned projection volume.
        ProjectionPlacement placement = ProjectionPlacement::Terrain;

        //! Fit a terrain-placed perspective decal's near/far range to its
        //! terrain intersection and optical field of view.
        bool computeClipRange = true;

        //! Construct a default decal
        Decal() = default;

        //! Construct a decal with a style attached to a different entity
        Decal(entt::entity styleEntity) : 
            Component<Decal>(), style(styleEntity) {}
    };
}
