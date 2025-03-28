/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/ecs/Component.h>
#include <rocky/GeoPoint.h>
#include <optional>

namespace ROCKY_NAMESPACE
{
    /**
    * Spatial transformation component.
    * Create with
    *   auto& transform = registry.emplace<Transform>(entity);
    *
    * A Transform may be safely updated asynchronously.
    */
    struct Transform : public RevisionedComponent
    {
        //! Georeferenced position
        GeoPoint position;

        //! Optional radius of the object (meters), which is used for culling.
        double radius = 0.0;

        //! Optional local matrix for rotation, offset, etc. This matrix is
        //! applied relative to the position AND to the topcentric ENU tangent plane
        //! when topocentric is set to true.
        glm::dmat4 localMatrix = glm::dmat4(1.0);

        //! Whether the localMatrix is relative to a local tangent plane at
        //! "position", versus a simple translated reference frame. Setting this
        //! to false will slightly improve performance.
        bool topocentric = true;

        //! True if objects positioned with this transform should be invisible
        //! if they are below the visible horizon.
        bool horizonCulled = true;

        //! True if objects positioned with this transform should be clipped
        //! to the view frustum.
        bool frustumCulled = true;

        //! Activates pointer stability for this component
        static constexpr auto in_place_delete = true;
    };
}
