/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Color.h>
#include <rocky_vsg/Common.h>
#include <rocky/Config.h>

namespace ROCKY_NAMESPACE
{
    class TerrainSettings
    {
    public:
        TerrainSettings(const Config& conf);

        void saveToConfig(Config& conf) const;

        //! Size of each dimension of each terrain tile, in verts.
        //! Ideally this will be a power of 2 plus 1, i.e.: a number X
        //! such that X = (2^Y)+1 where Y is an integer >= 1.
        optional<unsigned> tileSize = 17;

        //! The minimum tile LOD range as a factor of a tile's radius.
        //! This only applies when using distance-to-tile as a LOD technique.
        optional<float> minTileRangeFactor = 7.0;

        //! Acceptable error, in pixels, when rendering terrain tiles.
        optional<float> screenSpaceError = 150.0f;

        //! The maximum level of detail to which the terrain should subdivide.
        optional<unsigned> maxLevelOfDetail = 19;

        //! The level of detail at which the terrain should begin.
        optional<unsigned> minLevelOfDetail = 0;

        //! Whether the terrain engine will be using GPU tessellation shaders.
        optional<bool> gpuTessellation = false;

        //! GPU tessellation level
        optional<float> tessellationLevel = 2.5f;

        //! Maximum range in meters to apply GPU tessellation
        optional<float> tessellationRange = 75.0f;

        //! Minimum number of frames before unused terrain data is eligible to expire
        optional<unsigned> minFramesBeforeUnload = 0;

        //! Minimum time (seconds) before unused terrain data is eligible to expire
        optional<double> minSecondsBeforeUnload = 0.0;

        //! Minimun range (distance from camera) beyond which unused terrain data 
        //! is eligible to expire
        optional<float> minRangeBeforeUnload = 0.0f;

        //! Maximum number of terrain tiles to unload/expire each frame.
        optional<unsigned> maxTilesToUnloadPerFrame = ~0;

        //! Minimum number of terrain tiles to keep in memory before expiring usused data
        optional<unsigned> minResidentTilesBeforeUnload = 0;

        //! Whether the terrain should cast shadows on itself
        optional<bool> castShadows = false;

        //! Size of the tile, in pixels, when using rangeMode = PIXEL_SIZE_ON_SCREEN
        optional<float> tilePixelSize = 256.0f;

        //! Ratio of skirt height to tile width. The "skirt" is geometry extending
        //! down from the edge of terrain tiles meant to hide cracks between adjacent
        //! levels of detail. A value of 0 means no skirt.
        optional<float> skirtRatio = 0.0f;

        //! Color of the untextured globe (where no imagery is displayed)
        optional<Color> color = Color::White;

        //! Whether to generate normal map textures. Default is true
        optional<bool> useNormalMaps = true;

        //! Whether to average normal vectors on tile boundaries. Doing so reduces the
        //! the appearance of seams when using lighting, but requires extra CPU work.
        optional<bool> normalizeEdges = false;

        //! Whether to morph terrain data between terrain tile LODs.
        //! This feature is not available when using screen-space error LOD
        optional<bool> morphTerrain = false;

        //! Whether to morph imagery between terrain tile LODs.
        //! This feature is not available when using screen-space error LOD
        optional<bool> morphImagery = false;

        //! Target concurrency of terrain data loading operations.
        optional<unsigned> concurrency = 4;
    };
}
