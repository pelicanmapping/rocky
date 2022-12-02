/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Heightfield.h>
#include <rocky/GeoExtent.h>
#include <rocky/Units.h>

namespace rocky
{
    /**
     * An equipotential surface representing a gravitational model of the
     * planet's surface. Each value in the geoid's height field is an offset
     * from the reference ellipsoid.
     */
    class ROCKY_EXPORT Geoid :
        public Inherit<Object, Geoid>
    {
    public:
        std::string name;
        shared_ptr<Heightfield> heightfield;
        Units units;

        //! Construct
        Geoid(
            const std::string& name,
            shared_ptr<Heightfield> hf,
            const Units& units);

        //! Queries the geoid for the height offset at the specified geodetic
        //! coordinates (in degrees).
        float getHeight(
            double lat_deg,
            double lon_deg, 
            Heightfield::Interpolation interp = Heightfield::BILINEAR) const;

        //! Whether this is a valid object to use
        bool valid() const;
    };
}
