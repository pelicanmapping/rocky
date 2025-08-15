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
     * Profile describes a quadtree tiling structure along with its geospatial
     * reference. Profiles are for tiling purposes, so even though they contain
     * an SRS (for referencing) any vertical datum is ignored for the purposes
     * of tiling and Profile equivalency.
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
        //! @param subprofiles Optional subprofiles that make up a composite profile
        Profile(
            const SRS& srs,
            const Box& bounds = Box(),
            unsigned x_tiles_at_root = 0,
            unsigned y_tiles_at_root = 0,
            const std::vector<Profile>& subprofiles = {});

        // copy/move ops
        Profile(const Profile& rhs) = default;
        Profile& operator=(const Profile& rhs) = default;
        // NO move constructors since we use global instances of Profile

        //! @return True if the profile is properly initialized.
        bool valid() const;

        //! @return Extent of the profile (in the profile's SRS)
        const GeoExtent& extent() const;

        //! @return Extent of the profile in geodetic coordinates (long, lat degrees)
        const GeoExtent& geodeticExtent() const;
        
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
        //! Note: The SRS vertical datum is NOT considered when testing for equivalence.
        bool operator == (const Profile& rhs) const {
            return equivalentTo(rhs);
        }
        bool operator != (const Profile& rhs) const {
            return !equivalentTo(rhs);
        }

        //! Gets the tile dimensions at the given lod, in the profile's SRS units.
        using TileDims = struct { double x, y; };
        TileDims tileDimensions(unsigned lod) const;

        //! The number wide and high at the given lod
        using NumTiles = struct { std::uint32_t x, y; };
        NumTiles numTiles(unsigned lod) const;

        //! Returns a readable description of the profile.
        std::string to_json() const;

        //! Populate from a json string
        void from_json(const std::string& json);

        //! Given a LOD-0 tile height, determine the LOD in this Profile that
        //! most closely houses a tile with that height.
        unsigned levelOfDetail(double tileHeight) const;

        //! Makes a clone of this profile but replaces the SRS with a custom one.
        Profile overrideSRS(const SRS&) const;

        //! Well-known name of this profile, if it has one. These include
        //! global-geodetic, spherical-meractor, and plate-carree.
        const std::string& wellKnownName() const;

        //! Readable string represeting this profile as best we can
        std::string toReadableString() const;

        //! Whether this is a composite profile
        inline bool isComposite() const;

        //! Access composite profile components.
        inline std::vector<Profile>& subprofiles();
        inline const std::vector<Profile>& subprofiles() const;

        //! Get the hash code for this profile
        inline std::size_t hash() const;

    protected:

        struct Data
        {
            std::string wellKnownName;
            GeoExtent   extent;
            GeoExtent   geodeticExtent;
            unsigned    numTilesBaseX = 1u;
            unsigned    numTilesBaseY = 1u;
            std::size_t hash = 0;
            std::vector<Profile> subprofiles;
        };
        std::shared_ptr<Data> _shared;

        void setup(const std::string& wellKnownName);
        void setup(const SRS&, const Box& bounds, unsigned dim_x, unsigned dim_y, const std::vector<Profile> & = {});
    };


    // inlines
    inline auto Profile::hash() const -> std::size_t {
        return _shared->hash;
    }

    inline auto Profile::isComposite() const -> bool {
        return !_shared->subprofiles.empty();
    }

    inline auto Profile::subprofiles() -> std::vector<Profile>& {
        return _shared->subprofiles;
    }

    inline auto Profile::subprofiles() const -> const std::vector<Profile>& {
        return _shared->subprofiles;
    }
}

namespace std {
    // std::hash specialization for Profile
    template<> struct hash<rocky::Profile> {
        inline size_t operator()(const rocky::Profile& value) const {
            return value.hash();
        }
    };
}
