/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Units.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    class DateTime;

    /**
     * Location of a celestial body relative to the Earth.
     */
    struct CelestialBody
    {
        Angle rightAscension;
        Angle declination;
        Angle latitude;
        Angle longitude;
        Distance altitude;
        dvec3 geocentric;
        dvec3 eci;
    };

    /**
     * An Ephemeris gives the positions of naturally occurring astronimical
     * objects; in that case, the sun and the moon. Also includes some
     * related utility functions.
     */
    class ROCKY_EXPORT Ephemeris
    {
    public:

        //! Return the sun's position for a given date and time.
        virtual CelestialBody sunPosition(const DateTime& dt) const;

        //! Return the moon's position for a given date and time.
        virtual CelestialBody moonPosition(const DateTime& dt) const;
    };

}
