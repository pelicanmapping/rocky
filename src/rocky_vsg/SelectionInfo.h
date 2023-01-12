/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky/TileKey.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * SelectionInfo is a data structure that holds the LOD distance switching
     * information for the terrain, to support paging and LOD morphing.
     * This is calculated once when the terrain is first created.
     */
    class SelectionInfo
    {
    public:
        struct LOD
        {
            double _visibilityRange;
            double _morphStart;
            double _morphEnd;
            unsigned _minValidTY, _maxValidTY;
        };

    public:
        SelectionInfo() : _firstLOD(0) { }

        //! Initialize the selection into LODs
        void initialize(
            unsigned firstLod,
            unsigned maxLod,
            const Profile& profile,
            double mtrf,
            bool restrictPolarSubdivision);

        //! Number of LODs
        unsigned getNumLODs(void) const {
            return _lods.size();
        }

        //! Visibility and morphing information for a specific LOD
        const LOD& levelOfDetail(unsigned lod) const;

        //! Fetch the effective visibility range and morphing range for a key
        void get(
            const TileKey& key,
            float& out_range,
            float& out_startMorphRange,
            float& out_endMorphRange) const;

        //! Get just the visibility range for a TileKey.
        //! inlined for speed (SL measured)
        inline float getRange(const TileKey& key) const {
            const LOD& lod = _lods[key.levelOfDetail()];
            if (key.tileY() >= lod._minValidTY && key.tileY() <= lod._maxValidTY)
            {
                return lod._visibilityRange;
            }
            return 0.0f;
        }

    private:
        std::vector<LOD>    _lods;
        unsigned            _firstLOD;
        static const double _morphStartRatio;
    };
}
