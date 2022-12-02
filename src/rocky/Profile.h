/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Config.h>
#include <rocky/GeoExtent.h>
#include <vector>

namespace rocky
{
    class TileKey;

#if 0
    /**
     * Configuration options for initializing a Profile.
     * TODO: refactor into Profile::Options
     */
    class ROCKY_EXPORT ProfileOptions : public ConfigOptions
    {
    public:
        ProfileOptions( const ConfigOptions& options =ConfigOptions() );
        ProfileOptions( const std::string& namedProfile );

        /** dtor */
        virtual ~ProfileOptions() { }

        /** Returns true if this configuration is well-defined and usable */
        bool defined() const;

    public: // properties


        //! optional Well-Known Profile string.
        ROCKY_OPTION(std::string, namedProfile);
        //! spatial reference system initialization string to use for the profile. */
        ROCKY_OPTION(std::string, srsString);
        //! Gets the vertical spatial reference init string for this profile.
        ROCKY_OPTION(std::string, vsrsString);
        //! Geospatial bounds for this profile's extent
        ROCKY_OPTION(Box, bounds);
        //! Number of tiles in the X axis at LOD 0
        ROCKY_OPTION(int, numTilesWideAtLod0);
        //! Number of tiles on the Y axis at LOD 0
        ROCKY_OPTION(int, numTilesHighAtLod0);

    public:
        Config getConfig() const;

    protected:
        virtual void mergeConfig( const Config& conf );

    private:
        void fromConfig( const Config& conf );
    };
}
ROCKY_SPECIALIZE_CONFIG(rocky::ProfileOptions);
#endif


    /**
     * A "profile" defines the layout of a data source. The profile conveys the
     * spatial reference system (SRS), the geospatial extents within that SRS, and
     * the tiling scheme.
     */
    class ROCKY_EXPORT Profile :
        public Inherit<Object, Profile>
    {
    public:
        // well knowns
        static const std::string GLOBAL_GEODETIC;
        static const std::string GLOBAL_MERCATOR;
        static const std::string SPHERICAL_MERCATOR;
        static const std::string PLATE_CARREE;

    public:

        //! Wheter the profile is properly initialized.
        bool valid() const;

        //! Gets the extent of the profile (in the profile's SRS)
        const GeoExtent& getExtent() const;

        //! Gets the extent of the profile (in lat/long.)
        const GeoExtent& getLatLongExtent() const;
        
        //! spatial reference system underlying this profile.
        shared_ptr<SRS> getSRS() const;

        //! Given an x-resolution, specified in the profile's SRS units, calculates and
        //! returns the closest LOD level.
        unsigned int getLevelOfDetailForHorizResolution(
            double resolution,
            int tileSize ) const;

        //! tile keys that comprise the tiles at the root (LOD 0) of this
        //! profile. Same as calling getAllKeysAtLOD(0).
        static void getRootKeys(
            shared_ptr<Profile> target_profile,
            std::vector<TileKey>& out_keys);

        //! Gets all the tile keys at the specified LOD.
        static void getAllKeysAtLOD(
            unsigned lod,
            shared_ptr<Profile> target_profile,
            std::vector<TileKey>& out_keys);

        //! Calculates an extent given a tile location in this profile.
        virtual GeoExtent calculateExtent(
            unsigned lod,
            unsigned tileX,
            unsigned tileY) const;

        //! Gets whether the two profiles can be treated as equivalent.
        //! @param rhs
        //!   Comparison profile
        bool isEquivalentTo(Profile::ptr rhs) const;

        /**
         * Gets whether the two profiles can be treated as equivalent (without regard
         * for any vertical datum information - i.e., still returns true if the SRS
         * vertical datums are different)
         * @param rhs
         *      Comparison profile
         */
        bool isHorizEquivalentTo(Profile::ptr rhs) const;

        /**
         *Gets the tile dimensions at the given lod.
         */
        void getTileDimensions(
            unsigned lod,
            double& out_width,
            double& out_height) const;

        /**
         *Gets the number wide and high at the given lod
         */
        void getNumTiles(
            unsigned lod,
            unsigned& out_tiles_wide,
            unsigned int& out_tiles_high) const;

        /** 
         * Clamps the incoming extents to the extents of this profile, and then converts the 
         * clamped extents to this profile's SRS, and returns the result. Returned GeoExtent::INVALID
         * if the transformation fails.
         */
        GeoExtent clampAndTransformExtent( const GeoExtent& input, bool* out_clamped =0L ) const;

        /**
         * Creates a tile key for a tile that contains the input location at the specified LOD.
         * Express the coordinates in the profile's SRS.
         * Returns TileKey::INVALID if the point lies outside the profile's extents.
         */
        //TODO: Move to TileKey
        //TileKey createTileKey( double x, double y, unsigned int level ) const;

        //! Creates a tile key for a tile that contains the input location.
        //! @param point Point at which to create the tile key
        //! @param lod LOD at which to create the tile key
        //! @return tile key containing the point
        //TileKey createTileKey(const GeoPoint& point, unsigned level) const;

        /**
         * Returns a readable description of the profile.
         */
        std::string toString() const;

        /**
         * Builds and returns a ProfileOptions for this profile
         */
        //ProfileOptions toProfileOptions() const;

        /**
         * Returns a signature hash code unique to this profile.
         */
        const std::string& getFullSignature() const { return _fullSignature; }

        /**
         * Returns a signature hash code that uniquely identifies this profile
         * without including any vertical datum information. This is useful for
         * seeing if two profiles are horizontally compatible.
         */
        const std::string& getHorizSignature() const { return _horizSignature; }

        /**
         * Given another Profile and an LOD in that Profile, determine 
         * the LOD in this Profile that is nearly equivalent.
         */
        virtual unsigned getEquivalentLOD(Profile::ptr, unsigned lod) const;

        /**
         * Given a LOD-0 tile height, determine the LOD in this Profile that
         * most closely houses a tile with that height.
         */
        virtual unsigned getLOD(double tileHeight) const;

        //! Get the hash code for this profile
        std::size_t hash() const { return _hash; }

        /**
         * Given an input extent, translate it into one or more
         * GeoExtents in this profile.
         */
        virtual bool transformAndExtractContiguousExtents(
            const GeoExtent& input,
            std::vector<GeoExtent>& output) const;


        Config getConfig() const;


    public:

        /**
         * Makes a clone of this profile but replaces the SRS with a custom one.
         */
        Profile::ptr overrideSRS(shared_ptr<SRS>) const;

    protected:

        void setup(
            const std::string& wellKnownName);

        void setup(
            shared_ptr<SRS> srs,
            const Box& bounds,
            unsigned dim_x,
            unsigned dim_y );

    protected:

        std::string _wellKnownName;
        GeoExtent   _extent;
        GeoExtent   _latlong_extent;
        unsigned    _numTilesWideAtLod0;
        unsigned    _numTilesHighAtLod0;
        std::string _fullSignature;
        std::string _horizSignature;
        std::size_t _hash;

    public:

        explicit Profile(const Config&);

        explicit Profile(const std::string& well_known_name);

        Profile(
            shared_ptr<SRS> srs,
            const Box& bounds = Box(),
            unsigned x_tiles_at_lod0 = 0,
            unsigned y_tiles_at_lod0 = 0);
    };
}

namespace std {
    // std::hash specialization for Profile
    template<> struct hash<rocky::Profile> {
        inline size_t operator()(const rocky::Profile& value) const {
            return value.hash();
        }
    };
}
