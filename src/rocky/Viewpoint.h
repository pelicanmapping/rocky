/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Config.h>
#include <rocky/GeoPoint.h>
#include <rocky/Units.h>

namespace rocky
{
    /**
     * Viewpoint stores a focal point (or a focal node) and camera parameters
     * relative to that point.
     */
    class ROCKY_EXPORT Viewpoint
    {
    public:
        optional<std::string> name;
        optional<GeoPoint> point;
        optional<Angle> heading;
        optional<Angle> pitch;
        optional<Distance> range;
        optional<dvec3> posOffset;

    public:
        Viewpoint();

        //! Deserialize a Config into this viewpoint object.
        Viewpoint(const Config& conf);

    public:

        //! Returns true if this viewpoint contains either a valid focal point or
        //! a valid tracked node.
        bool valid() const;

        //! printable string with the viewpoint data
        std::string toString() const;

        //! Serialize this viewpoint to a config object.
        Config getConfig() const;
    };

} // namespace rocky
