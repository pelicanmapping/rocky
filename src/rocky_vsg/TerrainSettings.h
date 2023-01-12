/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Color.h>
#include <rocky_vsg/Common.h>

namespace ROCKY_NAMESPACE
{
    class TerrainSettings
    {
    public:
        TerrainSettings(const Config& conf);

        void saveToConfig(Config& conf) const;

        //! Size of each dimension of each terrain tile, in verts.
        //! Ideally this will be a power of 2 plus 1, i.e.: a number X
        //! such that X = (2^Y)+1 where Y is an integer >= 1. Default=17.
        optional<unsigned> tileSize;

        //! The minimum tile LOD range as a factor of a tile's radius. Default = 7.0
        optional<float> minTileRangeFactor;

        //! Whether cluster culling is enabled on terrain tiles. Deafult=true
        optional<bool> clusterCulling;

        //! (Legacy property)
        //! The maximum level of detail to which the terrain should subdivide.
        //! If you leave this unset the terrain will subdivide until the map layers
        //! stop providing data (default behavior). If you set a value, the terrain
        //! will stop subdividing at the specified LOD even if higher-resolution
        //! data is available. (It still might stop subdividing before it reaches
        //! this level if data runs out
        optional<unsigned> maxLOD;

        //! (Legacy property)
        //! The minimum level of detail to which the terrain should subdivide (no matter what).
        //! If you leave this unset, the terrain will subdivide until the map layers
        //! stop providing data (default behavior). If you set a value, the terrain will subdivide
        //! to the specified LOD no matter what (and may continue farther if higher-resolution
        //! data is available).
        optional<unsigned> minLOD;

        //! (Legacy property)
        //! The lowest LOD to display. By default, the terrain begins at LOD 0.
        //! Set this to start the terrain tile mesh at a higher LOD.
        //! Don't set this TOO high though or you will run into memory problems.
        optional<unsigned> firstLOD;

        //! Whether the terrain engine will be using GPU tessellation shaders.
        optional<bool> gpuTessellation;

        //! GPU tessellation level
        optional<float> tessellationLevel;

        //! Maximum range in meters to apply GPU tessellation
        optional<float> tessellationRange;

        //! Whether to activate debugging mode
        optional<bool> debugMode;

        //! Render bin number for the terrain
        //void setRenderBinNumber(const int& value);
        //const int& getRenderBinNumber() const;

        //! Minimum number of frames before unused terrain data is eligible to expire
        optional<unsigned> minFramesBeforeUnload;

        //! Minimum time (seconds) before unused terrain data is eligible to expire
        optional<double> minSecondsBeforeUnload;

        //! Minimun range (distance from camera) beyond which unused terrain data 
        //! is eligible to expire
        optional<float> minRangeBeforeUnload;

        //! Maximum number of terrain tiles to unload/expire each frame.
        optional<unsigned> maxTilesToUnloadPerFrame;

        //! Minimum number of terrain tiles to keep in memory before expiring usused data
        optional<unsigned> minResidentTilesBeforeUnload;

        //! Whether the terrain should cast shadows - default is false
        optional<bool> castShadows;

        //! Mode to use when calculating LOD switching distances.
        //! Choices are DISTANCE_FROM_EYE_POINT (default) or PIXEL_SIZE_ON_SCREEN
        //void setRangeMode(const osg::LOD::RangeMode& value);
        //const osg::LOD::RangeMode& getRangeMode() const;

        //! Size of the tile, in pixels, when using rangeMode = PIXEL_SIZE_ON_SCREEN
        optional<float> tilePixelSize;

        //! Ratio of skirt height to tile width. The "skirt" is geometry extending
        //! down from the edge of terrain tiles meant to hide cracks between adjacent
        //! levels of detail. Default is 0 (no skirt).
        optional<float> skirtRatio;

        //! Color of the untextured globe (where no imagery is displayed) (default is white)
        optional<Color> color;

        //! Whether to generate normal map textures. Default is true
        optional<bool> useNormalMaps;

        //! Whether to average normal vectors on tile boundaries. Doing so reduces the
        //! the appearance of seams when using lighting, but requires extra CPU work.
        optional<bool> normalizeEdges;

        //! Whether to morph terrain data between terrain tile LODs.
        //! This feature is not available when rangeMode is PIXEL_SIZE_ON_SCREEN
        optional<bool> morphTerrain;

        //! Whether to morph imagery between terrain tile LODs.
        //! This feature is not available when rangeMode is PIXEL_SIZE_ON_SCREEN
        optional<bool> morphImagery;

        //! Maximum number of tile data merges permitted per frame. 0 = infinity.
        //void setMergesPerFrame(const unsigned& value);
        //const unsigned& getMergesPerFrame() const;

        //! Scale factor for background loading priority of terrain tiles.
        //! Default = 1.0. Make it higher to prioritize terrain loading over
        //! other modules.
        //void setPriorityScale(const float& value);
        //const float& getPriorityScale() const;

        //! Texture compression to use by default on terrain image textures
        optional<std::string> textureCompressionMethod;

        //! Target concurrency of terrain data loading operations. Default = 4.
        optional<unsigned> concurrency;

        //! Screen space error for PIXEL SIZE ON SCREEN LOD mode
        optional<float> screenSpaceError;
    };
}
