/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Status.h>
#include <rocky/IOTypes.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    class Feature;

    class ROCKY_EXPORT Geocoder
    {
    public:
        Geocoder() = default;

        Result<std::vector<Feature>> geocode(const std::string& location, IOOptions& io) const;
    };
}
