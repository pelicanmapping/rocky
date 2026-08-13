/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Math.h>
#include <rocky/Rendering.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{

    //! Component that represents an optical device like a camera,
    //! sensor, or projector that processes light through a lens.
    struct Optics
    {
        enum class Projection
        {
            Perspective,
            Orthographic
        };

        Projection projection = Projection::Orthographic;

        //! Matrix that transforms the rotation, scale, or positional offset of the
        //! optics relative to its owning Transform. For an orthographic projection,
        //! the transformed unit cube is the projection volume.
        glm::dmat4 pose = glm::dmat4(1.0);

        //! Distance from the lens to the focal plane in meters.
        double focalDistance = 1.0;

        //! Lens parameters (perspective projection)
        double fovY = 45.0; // degrees
        double aspectRatio = 1.0;

        // When calculating near/far clip planes for a persepctive projection:
        // near = focalDistance * nearScale + nearBias
        // far = focalDistance * farScale + farBias
        double nearScale = 1.0;
        double farScale = 1.0;
        double nearBias = 0.0;
        double farBias = 0.0;

        //! Whether to attempt to automatically compute a focal distance based on
        //! scene geometry. For an orthographic projection, a successful terrain
        //! intersection also recenters the projection volume on the focal point.
        bool autoComputeFocalDistance = true;

        //! For a perspective projection, whether to automatically compute near/far
        //! distances based on scene geometry and the focal distance.
        bool autoComputeNearFar = true;
    };

    struct OpticsViewDetail
    {
        glm::dvec3 focalPoint;
        double focalDistance = 1.0;
        double nearDistance = 1.0;
        double farDistance = 1.0;
        bool focalPointValid = false;
        bool autoComputeCacheValid = false;
        std::uint64_t lastTerrainRevision = 0u;
        glm::dmat4 lastAutoComputeWorld = glm::dmat4(1.0);
    };

    struct OpticsDetail
    {
        ViewLocal<OpticsViewDetail> views;
    };
}
