/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/GeoPoint.h>
#include <rocky/Units.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Viewpoint stores a focal point (or a focal node) and camera parameters
     * relative to that point.
     */
    class ROCKY_EXPORT Viewpoint
    {
    public:
        //! Readable name
        optional<std::string> name;

        //! Static focal point (if set)
        GeoPoint point;

        //! Dynamic focal point (if set)
        std::shared_ptr<PositionedObject> target;

        //! Heading of the viewer relative to north
        optional<Angle> heading = Angle(0.0, Units::DEGREES);

        //! Pitch of the viewer relative to the ground
        optional<Angle> pitch = Angle(-90, Units::DEGREES);

        //! Distance of the viewer to the target
        optional<Distance> range = Distance(10.0, Units::KILOMETERS);

        //! Offset in cartesian space from the focal point
        optional<glm::dvec3> positionOffset = glm::dvec3(0, 0, 0);

    public:
        //! Construct an empty viewpoint
        Viewpoint() { }

        Viewpoint(const Viewpoint&) = default;

        //! The focal point
        inline const GeoPoint& position() const {
            return (target ? target->objectPosition() : point);
        }

        //! If this a valid viewpoint?
        inline bool valid() const {
            return
                point.valid() ||
                (target != nullptr && target->objectPosition().valid());
        }
    };

} // namespace ROCKY_NAMESPACE
