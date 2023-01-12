/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
#include <rocky/Color.h>
#include <string>

namespace ROCKY_NAMESPACE
{
    // Options structure for a terrain engine (internal)
    class ROCKY_EXPORT TerrainOptions : public DriverConfigOptions
    {
    public:
        META_ConfigOptions(rocky, TerrainOptions, DriverConfigOptions);
        ROCKY_OPTION(int, tileSize);
        ROCKY_OPTION(float, minTileRangeFactor);
        ROCKY_OPTION(unsigned, maxLOD);
        ROCKY_OPTION(unsigned, minLOD);
        ROCKY_OPTION(unsigned, firstLOD);
        ROCKY_OPTION(float, tessellationLevel);
        ROCKY_OPTION(float, tessellationRange);
        ROCKY_OPTION(unsigned, minExpiryFrames);
        ROCKY_OPTION(double, minExpiryTime);
        ROCKY_OPTION(float, minExpiryRange);
        ROCKY_OPTION(unsigned, minResidentTiles);
        ROCKY_OPTION(bool, castShadows);
        ROCKY_OPTION(float, tilePixelSize);
        ROCKY_OPTION(float, heightfieldSkirtRatio);
        ROCKY_OPTION(Color, color);
        ROCKY_OPTION(bool, morphTerrain);
        ROCKY_OPTION(bool, morphImagery);
        ROCKY_OPTION(unsigned, concurrency);
        ROCKY_OPTION(float, screenSpaceError);

        virtual Config getConfig() const;
    private:
        void fromConfig(const Config&);
    };

    class ROCKY_EXPORT TerrainOptionsAPI
    {
    public:
        //! Size of each dimension of each terrain tile, in verts.
        //! Ideally this will be a power of 2 plus 1, i.e.: a number X
        //! such that X = (2^Y)+1 where Y is an integer >= 1. Default=17.
        void setTileSize(const int& value);
        const int& tileSize() const;

        //! The minimum tile LOD range as a factor of a tile's radius. Default = 7.0
        void setMinTileRangeFactor(const float& value);
        const float& minTileRangeFactor() const;

        //! (Legacy property)
        //! The maximum level of detail to which the terrain should subdivide.
        //! If you leave this unset the terrain will subdivide until the map layers
        //! stop providing data (default behavior). If you set a value, the terrain
        //! will stop subdividing at the specified LOD even if higher-resolution
        //! data is available. (It still might stop subdividing before it reaches
        //! this level if data runs out
        void setMaxLOD(const unsigned& value);
        const unsigned&maxLOD() const;

        //! (Legacy property)
        //! The minimum level of detail to which the terrain should subdivide (no matter what).
        //! If you leave this unset, the terrain will subdivide until the map layers
        //! stop providing data (default behavior). If you set a value, the terrain will subdivide
        //! to the specified LOD no matter what (and may continue farther if higher-resolution
        //! data is available).
        void setMinLOD(const unsigned& value);
        const unsigned& minLOD() const;

        //! (Legacy property)
        //! The lowest LOD to display. By default, the terrain begins at LOD 0.
        //! Set this to start the terrain tile mesh at a higher LOD.
        //! Don't set this TOO high though or you will run into memory problems.
        void setFirstLOD(const unsigned& value);
        const unsigned& firstLOD() const;

        //! GPU tessellation level
        void setTessellationLevel(const float& value);
        const float& tessellationLevel() const;

        //! Maximum range in meters to apply GPU tessellation
        void setTessellationRange(const float& value);
        const float& tessellationRange() const;

        //! Minimum number of frames before unused terrain data is eligible to expire
        void setMinExpiryFrames(const unsigned& value);
        const unsigned& minExpiryFrames() const;

        //! Minimum time (seconds) before unused terrain data is eligible to expire
        void setMinExpiryTime(const double& value);
        const double& minExpiryTime() const;

        //! Minimun range (distance from camera) beyond which unused terrain data 
        //! is eligible to expire
        void setMinExpiryRange(const float& value);
        const float& minExpiryRange() const;

        //! Maximum number of terrain tiles to unload/expire each frame.
        void setMaxTilesToUnloadPerFrame(const unsigned& value);
        const unsigned& maxTilesToUnloadPerFrame() const;

        //! Minimum number of terrain tiles to keep in memory before expiring usused data
        void setMinResidentTiles(const unsigned& value);
        const unsigned& minResidentTiles() const;

        //! Whether the terrain should cast shadows - default is false
        void setCastShadows(const bool& value);
        const bool& castShadows() const;

        //! Size of the tile, in pixels, when using rangeMode = PIXEL_SIZE_ON_SCREEN
        void setTilePixelSize(const float& value);
        const float& tilePixelSize() const;

        //! Ratio of skirt height to tile width. The "skirt" is geometry extending
        //! down from the edge of terrain tiles meant to hide cracks between adjacent
        //! levels of detail. Default is 0 (no skirt).
        void setHeightfieldSkirtRatio(const float& value);
        const float& heightfieldSkirtRatio() const;

        //! Color of the untextured globe (where no imagery is displayed) (default is white)
        void setColor(const Color& value);
        const Color& color() const;

        //! Whether to morph terrain data between terrain tile LODs.
        //! This feature is not available when rangeMode is PIXEL_SIZE_ON_SCREEN
        void setMorphTerrain(const bool& value);
        const bool& morphTerrain() const;

        //! Whether to morph imagery between terrain tile LODs.
        //! This feature is not available when rangeMode is PIXEL_SIZE_ON_SCREEN
        void setMorphImagery(const bool& value);
        const bool& morphImagery() const;

        //! Target concurrency of terrain data loading operations. Default = 4.
        void setConcurrency(const unsigned& value);
        const unsigned& concurrency() const;

        //! Screen space error for PIXEL SIZE ON SCREEN LOD mode
        void setScreenSpaceError(const float& value);
        const float& screenSpaceError() const;

    public: // Legacy support

        //! Sets the name of the terrain engine driver to use
        void setDriver(const std::string& name);

    private:
        friend class MapNode;
        friend class TerrainEngineNode;
        TerrainOptionsAPI(TerrainOptions*);
        TerrainOptions* _ptr;
        TerrainOptions& options() { return *_ptr; }
        const TerrainOptions& options() const { return *_ptr; }
    };

} // namespace ROCKY_NAMESPACE
