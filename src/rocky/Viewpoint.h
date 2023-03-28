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
    * Base class for any object that has a position on a Map.
    */
    class ROCKY_EXPORT MapObject
    {
    public:
        //! Center position of the object
        virtual const GeoPoint& position() const = 0;
    };

    /**
    * Simplest possible map object - just a position.
    */
    class ROCKY_EXPORT SimpleMapObject : public MapObject
    {
    public:
        GeoPoint point;

        const GeoPoint& position() const override {
            return point;
        }

        SimpleMapObject() { }
        SimpleMapObject(const SimpleMapObject&) = default;
        SimpleMapObject(const GeoPoint& point_) : point(point_) { }
    };

    /**
     * Viewpoint stores a focal point (or a focal node) and camera parameters
     * relative to that point.
     */
    class ROCKY_EXPORT Viewpoint
    {
    public:
        //! Readable name
        optional<std::string> name;

        //! What the viewer is looking at.
        std::shared_ptr<MapObject> target = nullptr;

        //! Heading of the viewer relative to north
        optional<Angle> heading = Angle(0.0, Units::DEGREES);

        //! Pitch of the viewer relative to the ground
        optional<Angle> pitch = Angle(-30, Units::DEGREES);

        //! Distance of the viewer to the target
        optional<Distance> range = Distance(10.0, Units::KILOMETERS);

        //! Offset in cartesian space from the focal point
        optional<dvec3> positionOffset = dvec3(0, 0, 0);

    public:
        Viewpoint() { }

        Viewpoint(const Viewpoint&) = default;

        //! If this a valid viewpoint?
        bool valid() const {
            return target != nullptr && target->position().valid();
        }
    };

} // namespace ROCKY_NAMESPACE
