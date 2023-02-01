/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

// For FLT_MAX
#include <limits.h>
#include <cfloat>

//#undef  NO_DATA_VALUE
//#define NO_DATA_VALUE -FLT_MAX

namespace ROCKY_NAMESPACE
{
    constexpr float NO_DATA_VALUE = -FLT_MAX;

#if 0
    /**
     * Types of interpolation between two geodetic locations.
     */
    enum GeoInterpolation
    {
        GEOGREAT_CIRCLE,
        GEORHUMB_LINE
    };
#endif

#if 0
    /**
     * Elevation stack sampling policy
     */
    enum ElevationSamplePolicy
    {
        SAMPLE_FIRST_VALID,
        SAMPLE_HIGHEST,
        SAMPLE_LOWEST,
        SAMPLE_AVERAGE
    };
#endif

#if 0
    /**
     * Elevation NO_DATA treatment policy
     */
    enum ElevationNoDataPolicy
    {
        NODATA_INTERPOLATE,     // interpolate across NO_DATA samples
        NODATA_MSL              // rewrite NO_DATA samples as MSL
    };
#endif

    /**
     * Indicates how to interpret a Z coordinate.
     */
    enum AltitudeMode
    {
        ALTMODE_ABSOLUTE,  // Z value is the absolute height above MSL/HAE.
        ALTMODE_RELATIVE   // Z value is the height above the terrain elevation.
    };
}
