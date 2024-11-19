/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Geoid.h"
#include "Heightfield.h"
#include "Units.h"
#include "GeoHeightfield.h"

#define LC "[Geoid] "

using namespace ROCKY_NAMESPACE;


Geoid::Geoid(
    const std::string& name_,
    std::shared_ptr<Heightfield> hf_,
    const Units& units_) :

    name(name_),
    heightfield(hf_),
    units(units_)
{
    //nop
}

bool
Geoid::valid() const
{
    return heightfield != nullptr;
}

float
Geoid::getHeight(
    double lat_deg,
    double lon_deg,
    Heightfield::Interpolation interp) const
{
    float result = 0.0f;

    if (valid())
    {
        const double width = 360.0;
        const double height = 180.0;
        double u = (lon_deg - (-180.0)) / width;
        double v = (lat_deg - (-90.0)) / height;
        result = heightfield->heightAtUV(u, v, interp);
    }

    return result;
}
