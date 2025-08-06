/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Profile.h>
#include <string>
#include <functional> // std::hash

namespace ROCKY_NAMESPACE
{
    class GeoPoint;

    /**
     * Uniquely identifies a single tile on the map, relative to a Profile.
     * Profiles have an origin of 0,0 at the top left.
     */
    class ROCKY_EXPORT TileKey
    {
    public:
        unsigned level;
        unsigned x;
        unsigned y;
        Profile profile;


        TileKey() = default;
        TileKey(const TileKey& rhs) = default;
        TileKey& operator = (const TileKey& rhs) = default;
        TileKey(TileKey&& rhs) noexcept;
        TileKey& operator = (TileKey&& rhs) noexcept;

        //! Creates a new TileKey with the given tile xy at the specified level of detail
        //! @param level The level of detail (subdivision recursion level) of the tile
        //! @param tile_x The x index of the tile
        //! @param tile_y The y index of the tile
        //! @param profile The profile for the tile
        TileKey(
            unsigned level,
            unsigned tile_x,
            unsigned tile_y,
            const Profile& profile);

        //! Compare two tilekeys for equality.
        inline bool operator == (const TileKey& rhs) const {
            return
                valid() == rhs.valid() &&
                level == rhs.level &&
                x == rhs.x &&
                y == rhs.y &&
                profile.equivalentTo(rhs.profile);
        }

        //! Compare two tilekeys for inequality
        inline bool operator != (const TileKey& rhs) const {
            return 
                valid() != rhs.valid() ||
                level != rhs.level ||
                x != rhs.x ||
                y != rhs.y ||
                !profile.equivalentTo(rhs.profile);
        }

        //! Sorts tilekeys
        inline bool operator < (const TileKey& rhs) const {
            if (level < rhs.level) return true;
            if (level > rhs.level) return false;
            if (x < rhs.x) return true;
            if (x > rhs.x) return false;
            if (y < rhs.y) return true;
            if (y > rhs.y) return false;
            return profile.hash() < rhs.profile.hash();
        }

        //! Canonical invalid tile key
        static TileKey INVALID;

        //! Gets the string representation of the key, formatted like:
        //! "lod/x/y"
        const std::string str() const;

        //! Whether this is a valid key.
        bool valid() const {
            return profile.valid();
        }

        //! Get the quadrant relative to this key's parent.
        unsigned getQuadrant() const;

        //! Gets a reference to the child key of this key in the specified
        //! quadrant (0, 1, 2, or 3).
        TileKey createChildKey(unsigned quadrant) const;

        //! Gets a scale/bias matrix for this key relative to its parent key
        //! Returns identity matrix for LOD = 0
        const glm::dmat4 scaleBiasMatrix() const;

        //! Creates and returns a key that represents the parent tile of this key.
        TileKey createParentKey() const;

        //! Convert this key to its parent
        //! @return true upon success, false if invalid
        bool makeParent();

        //! Creates and returns a key that represents an ancestor tile
        //! corresponding to the specified LOD.
        TileKey createAncestorKey(unsigned ancestorLod) const;

        //! Creates a key that represents this tile's neighbor at the same LOD. Wraps
        //! around in X and Y automatically. For example, xoffset=-1 yoffset=1 will
        //! give you the key for the tile SW of this tile.
        TileKey createNeighborKey(int xoffset, int yoffset) const;

        //! Gets the geospatial extents of the tile represented by this key.
        const GeoExtent extent() const;

        //! A string that encodes the tile key's lod, x, and y 
        std::string quadKey() const;

        //! Maps this tile key to another tile key in order to account in
        //! a resolution difference. For example: we are requesting data for
        //! a TileKey at LOD 4, at a tile size of 32. The source data's tile
        //! size if 256. That means the source data at LOD 1 will contain the
        //! data necessary to build the tile.
        //!
        //!   usage: TileKey tk = mapResolution(31, 256);
        //!
        //! We want a 31x31 tile. The source data is 256x256. This will return
        //! a TileKey that access the appropriate source data, which we will then
        //! need to crop to populate our tile.
        //!
        //! You can set "minimumLOD" if you don't want a key below a certain LOD.
        TileKey mapResolution(
            unsigned targetSize,
            unsigned sourceDataSize,
            unsigned minimumLOD =0) const;

        //! X and Y resolution (in profile units) for the given tile size
        std::pair<double,double> getResolutionForTileSize(
            unsigned tileSize) const;

        //! Creates a TileKey containing (x, y) in the requested profile.
        static TileKey createTileKeyContainingPoint(
            double x, double y, unsigned level,
            const Profile& profile);

        //! Creates a TileKey containing (x, y) in the requested profile.
        static TileKey createTileKeyContainingPoint(
            const GeoPoint& point,
            unsigned level,
            const Profile& profile);

        //! Gets the keys that intersect this TileKey in the requested profile.
        std::vector<TileKey> intersectingKeys(const Profile& profile) const;

        static std::vector<TileKey> intersectingKeys(
            const GeoExtent& extent,
            unsigned localLOD,
            const Profile& target_profile);
    };
}
