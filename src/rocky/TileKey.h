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
        //! Constructs an invalid TileKey.
        TileKey() { }

        //! Creates a new TileKey with the given tile xy at the specified level of detail
        //! @param lod
        //!     The level of detail (subdivision recursion level) of the tile
        //! @param tile_x
        //!     The x index of the tile
        //! @param tile_y
        //!     The y index of the tile
        //! @param profile
        //!     The profile for the tile
        TileKey(
            unsigned lod,
            unsigned tile_x,
            unsigned tile_y,
            const Profile& profile);

        //! Compare two tilekeys for equality.
        inline bool operator == (const TileKey& rhs) const {
            return
                valid() == rhs.valid() &&
                _lod == rhs._lod &&
                _x == rhs._x &&
                _y == rhs._y &&
                _profile.isHorizEquivalentTo(rhs._profile);
        }

        //! Compare two tilekeys for inequality
        inline bool operator != (const TileKey& rhs) const {
            return 
                valid() != rhs.valid() ||
                _lod != rhs._lod ||
                _x != rhs._x ||
                _y != rhs._y ||
                !_profile.isEquivalentTo(rhs._profile);
        }

        //! Sorts tilekeys, ignoring profiles
        inline bool operator < (const TileKey& rhs) const {
            if (_lod < rhs._lod) return true;
            if (_lod > rhs._lod) return false;
            if (_x < rhs._x) return true;
            if (_x > rhs._x) return false;
            if (_y < rhs._y) return true;
            if (_y > rhs._y) return false;
            return _profile.getHorizSignature() < rhs._profile.getHorizSignature();
        }

        //! Canonical invalid tile key
        static TileKey INVALID;

        //! Gets the string representation of the key, formatted like:
        //! "lod/x/y"
        const std::string str() const;

        //! Gets the profile within which this key is interpreted.
        const Profile& profile() const;

        //! Whether this is a valid key.
        bool valid() const {
            return _profile.valid();
        }

        //! Get the quadrant relative to this key's parent.
        unsigned getQuadrant() const;

        //! Gets a reference to the child key of this key in the specified
        //! quadrant (0, 1, 2, or 3).
        TileKey createChildKey(unsigned quadrant) const;

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

        //! Gets the level of detail of the tile represented by this key.
        unsigned getLevelOfDetail() const { return _lod; }
        unsigned getLOD() const { return _lod; }

        //! Gets the geospatial extents of the tile represented by this key.
        const GeoExtent extent() const;

        unsigned getTileX() const { return _x; }

        unsigned getTileY() const { return _y; }

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
        std::pair<double,double> getResolution(
            unsigned tileSize) const;

        //! Creates a TileKey containing (x, y) in the requested profile.
        static TileKey createTileKeyContainingPoint(
            double x, double y, unsigned lod,
            const Profile& profile);

        //! Creates a TileKey containing (x, y) in the requested profile.
        static TileKey createTileKeyContainingPoint(
            const GeoPoint& point,
            unsigned lod,
            const Profile& profile);

        //! Gets the keys that intersect this TileKey in the requested profile.
        void getIntersectingKeys(
            const Profile& profile,
            std::vector<TileKey>& output) const;

        static void getIntersectingKeys(
            const GeoExtent& extent,
            unsigned localLOD,
            const Profile& target_profile,
            std::vector<TileKey>& out_intersectingKeys);

        //! Convenience method to match this key.
        bool is(unsigned lod, unsigned x, unsigned y) const {
            return _lod == lod && _x == x && _y == y;
        }

    protected:
        unsigned _lod;
        unsigned _x;
        unsigned _y;
        Profile _profile;
        size_t _hash;
        void rehash();

    public:
        size_t hash() const { return _hash; }
    };
}

namespace std {
    // std::hash specialization for TileKey
    template<> struct hash<rocky::TileKey> {
        inline size_t operator()(const rocky::TileKey& value) const {
            return value.hash();
        }
    };
}
