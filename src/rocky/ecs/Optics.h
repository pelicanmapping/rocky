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

        //! Matrix that transforms the rotation or positional offset
        //! of the optics. Typically Optics is used in conjunction with a Transform
        //! component that will position it in the world.
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

        //! Whether to attempt to automatically compute
        //! a focal distance based on the scene geometry.
        bool autoComputeFocalDistance = true;

        //! When true, attemp to automatically compute near/far scale/bias values based on
        //! the scene geometry and the focal distance.
        bool autoComputeNearFar = true;
    };

    struct OpticsViewDetail
    {
        glm::dvec3 focalPoint;
        double focalDistance = 1.0;
        double nearDistance = 1.0;
        double farDistance = 1.0;
    };

    struct OpticsDetail
    {
        ViewLocal<OpticsViewDetail> views;
    };
}
