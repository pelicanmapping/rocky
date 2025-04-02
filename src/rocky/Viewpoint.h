/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/GeoPoint.h>
#include <rocky/Units.h>
#include <functional>

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
        option<std::string> name;

        //! Static focal point (if set)
        GeoPoint point;

        //! Function to get a focal point that's called every frame (if set)
        //! Set this to tether to a moving object.
        std::function<GeoPoint()> pointFunction;

        //! Dynamic focal point (if set)
        //std::shared_ptr<PositionedObject> target;

        //! Heading of the viewer relative to north
        option<Angle> heading = Angle(0.0, Units::DEGREES);

        //! Pitch of the viewer relative to the ground
        option<Angle> pitch = Angle(-90, Units::DEGREES);

        //! Distance of the viewer to the target
        option<Distance> range = Distance(10.0, Units::KILOMETERS);

        //! Offset in cartesian space from the focal point
        option<glm::dvec3> positionOffset = glm::dvec3(0, 0, 0);

    public:
        //! Construct an empty viewpoint
        Viewpoint() { }

        //! Default copy contstructor
        Viewpoint(const Viewpoint&) = default;

        //! The focal position
        GeoPoint position() const {
            if (pointFunction)
                return pointFunction();
            else
                return point;
        }

        //! Is this a valid viewpoint?
        inline bool valid() const {
            return point.valid() || pointFunction != nullptr;
        }
    };

} // namespace ROCKY_NAMESPACE
