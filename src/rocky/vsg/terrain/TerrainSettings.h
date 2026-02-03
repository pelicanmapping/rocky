/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Color.h>
#include <rocky/Result.h>
#include <rocky/vsg/Common.h>

namespace ROCKY_NAMESPACE
{
    /**
    * Settings controlling the terrain surface rendering and paging.
    */
    class TerrainSettings
    {
    public:
        TerrainSettings() = default;

        //! deserialize from JSON
        Result<> from_json(std::string_view JSON);

        //! serialize to JSON
        std::string to_json() const;

        //! Size of each dimension of each terrain tile, in verts.
        //! Ideally this will be a power of 2 plus 1, i.e.: a number X
        //! such that X = (2^Y)+1 where Y is an integer >= 1.
        option<unsigned> tileSize = 17;

        //! The minimum tile LOD range as a factor of a tile's radius.
        //! This only applies when using distance-to-tile as a LOD technique.
        option<float> minTileRangeFactor = 7.0;

        //! Acceptable error, in pixels, when rendering terrain tiles.
        option<float> pixelError = 128.0f;

        //! The maximum level of detail to which the terrain should subdivide.
        option<unsigned> maxLevel = 23;

        //! The level of detail at which the terrain should begin.
        option<unsigned> minLevel = 0;

        //! Size of the highest resolution imagery tile, in pixels
        option<float> tilePixelSize = 256.0f;

        //! Ratio of skirt height to tile width. The "skirt" is geometry extending
        //! down from the edge of terrain tiles meant to hide cracks between adjacent
        //! levels of detail. A value of 0 means no skirt.
        option<float> skirtRatio = 0.025f;

        //! Color of the untextured globe (where no imagery is displayed)
        option<Color> backgroundColor = Color("#08AEE0");

        //! Number of threads dedicated to loading terrain data
        option<unsigned> concurrency = 6;

        //! Whether to render a wireframe overlay on the terrain
        option<bool> wireOverlay = false;

        //! Whether to apply lighting to the terrain surface
        option<bool> lighting = false;

        //! Number of expired terrain tiles to cache in memory
        option<unsigned> tileCacheSize = 0u;

    public: // internal runtime settings, not serialized.

        //! TEMPORARY.
        //! To deal with multi-threaded Record (b/c of multiple command graphs)
        //! without using an unnecessary lock in the single-threaded case
        bool supportMultiThreadedRecord = false;
    };
}
