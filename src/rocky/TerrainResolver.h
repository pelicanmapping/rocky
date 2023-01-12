/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>

namespace ROCKY_NAMESPACE
{
    // TEMPORARY - replace
    class TerrainResolver
    {
    public:

        virtual bool getHeight(
            const SRS& srs,
            double                  x,
            double                  y,
            double*                 out_heightAboveMSL,
            double*                 out_heightAboveEllipsoid = 0L) const = 0;
    };
}

