/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoExtent.h>
#include <vector>
#include <string>

namespace ROCKY_NAMESPACE
{
    class TileKey;

    /**
     * A "profile" defines the layout of a data source. The profile conveys the
     * spatial reference system (SRS), the geospatial extents within that SRS, and
     * the tiling scheme.
     */
    class ROCKY_EXPORT Profile
    {
    public:
        //! Construct an empty, invalid profile
        Profile();

        //! Construct a profile from a well-know name or initialization string
        //! Can be one of
        //!   global-geodetic
        //!   spherical-mercator
        //!   plate-carree
        //!   moon
        //!   .. or any valid +proj initialization string
        explicit Profile(const std::string& well_known_name);

        //! Construct a profile
        //! @param srs Spatial reference system of the underlying tiles
        //! @param box Full extent of the profile (in the provided SRS)
        //! @param x_tiles_at_root Number of tiles in the X dimension at level of detail zero
        //! @param y_tiles_at_root Number of tiles in the Y dimension at level of detail zero
        Profile(
            const SRS& srs,
            const Box& bounds = Box(),
            unsigned x_tiles_at_root = 0,
            unsigned y_tiles_at_root = 0);

        // copy/move ops
        Profile(const Profile& rhs) = default;
        Profile& operator=(const Profile& rhs) = default;
        // NO move constructors since we use global instances of Profile

        //! @return True if the profile is properly initialized.
        bool valid() const;

        //! @return Extent of the profile (in the profile's SRS)
        const GeoExtent& extent() const;

        //! @return Extent of the profile in geographic coordinates (long, lat degrees)
        const GeoExtent& geographicExtent() const;
        
        //! @return Spatial reference system underlying this profile.
        const SRS& srs() const;

        //! @return Given an x-resolution, specified in the profile's SRS units, calculates and
        //! returns the closest LOD level.
        unsigned levelOfDetailForHorizResolution(double resolution, int tileSize) const;

        //! Tile keys that comprise the tiles at the root (LOD 0) of this
        //! profile. Same as calling getAllKeysAtLOD(0).
        //! @param target_profile Profile for which to query root keys
        //! @param out_keys Places keys in this vector
        std::vector<TileKey> rootKeys() const;

        //! Gets all the tile keys at the specified LOD.
        //! @param lod Level of detail for which to query keys
        //! @param target_profile Profile for which to query keys
        //! @param out_keys Places keys in this vector
        std::vector<TileKey> allKeysAtLOD(unsigned lod) const;

        //! @return Extent given a tile location in this profile.
        //! @param lod Level of detail for which to calculate tile extent
        //! @param tileX X tile index
        //! @param tile& Y tile index
        GeoExtent tileExtent(
            unsigned lod,
            unsigned tileX,
            unsigned tileY) const;

        //! Gets whether the two profiles can be treated as equivalent.
        //! @param rhs Comparison profile
        bool equivalentTo(const Profile& rhs) const;

        //! Whether two profiles are equivalent.
        bool operator == (const Profile& rhs) const {
            return equivalentTo(rhs);
        }
        bool operator != (const Profile& rhs) const {
            return !equivalentTo(rhs);
        }

        //! Gets whether the two profiles can be treated as equivalent (without regard
        //! for any vertical datum information - i.e., still returns true if the SRS
        //! vertical datums are different)
        //! @param rhs Comparison profile
        bool horizontallyEquivalentTo(const Profile& rhs) const;

        //! Gets the tile dimensions at the given lod, in the profile's SRS units.
        std::pair<double, double> tileDimensions(unsigned lod) const;

        //! The number wide and high at the given lod
        std::pair<unsigned, unsigned> numTiles(unsigned lod) const;

        //! Clamps the incoming extents to the extents of this profile, and then converts the 
        //! clamped extents to this profile's SRS, and returns the result. Returned GeoExtent::INVALID
        //! if the transformation fails.
        GeoExtent clampAndTransformExtent( const GeoExtent& input, bool* out_clamped =0L ) const;

        //! Returns a readable description of the profile.
        std::string to_json() const;

        //! Populate from a json string
        void from_json(const std::string& json);

        //! Given another Profile and an LOD in that Profile, determine 
        //! the LOD in this Profile that is nearly equivalent.
        unsigned equivalentLOD(const Profile&, unsigned lod) const;

        //! Given a LOD-0 tile height, determine the LOD in this Profile that
        //! most closely houses a tile with that height.
        unsigned levelOfDetail(double tileHeight) const;

        //! Get the hash code for this profile
        inline std::size_t hash() const;

        //! Given an input extent, translate it into one or more
        //! GeoExtents in this profile.
        bool transformAndExtractContiguousExtents(
            const GeoExtent& input,
            std::vector<GeoExtent>& output) const;

        //! Makes a clone of this profile but replaces the SRS with a custom one.
        Profile overrideSRS(const SRS&) const;

        //! Well-known name of this profile, if it has one. These include
        //! global-geodetic, spherical-meractor, and plate-carree.
        const std::string& wellKnownName() const;

        //! Readable string represeting this profile as best we can
        std::string toReadableString() const;

    protected:

        void setup(const std::string& wellKnownName);
        void setup(const SRS&, const Box& bounds, unsigned dim_x, unsigned dim_y);

    protected:

        struct Data
        {
            std::string wellKnownName;
            GeoExtent   extent;
            GeoExtent   geodeticExtent;
            unsigned    numTilesBaseX = 1u;
            unsigned    numTilesBaseY = 1u;
            std::size_t hash = 0;
        };
        std::shared_ptr<Data> _shared;
    };


    // inlines
    std::size_t Profile::hash() const { return _shared->hash; }
}

namespace std {
    // std::hash specialization for Profile
    template<> struct hash<rocky::Profile> {
        inline size_t operator()(const rocky::Profile& value) const {
            return value.hash();
        }
    };
}
