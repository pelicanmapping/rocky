/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <glm/glm.hpp>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS Component applying motion in a cartesian space
    */
    struct Motion
    {
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
    };

    /**
    * ECS Component applying motion along a great circle
    */
    struct MotionGreatCircle : public Motion
    {
        glm::dvec3 normalAxis;
    };
}
