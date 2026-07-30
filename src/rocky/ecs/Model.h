/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/URI.h>
#include <rocky/ecs/Component.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    /**
    * Model component.
    * Represents a loadable 3D model.
    */
    struct Model : public Component<Model>
    {
        //! URI of model to load
        URI uri;

        //! Radius of model, once loaded
        float radius;

        //! Local matrix to apply to model after load, e.g. to rotate into an ENU frame
        std::optional<glm::dmat4> localMatrix;

        //! Failure information if the model failed to materialize.
        std::optional<Failure> error;
    };
}
