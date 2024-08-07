/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileKey.h"
#include "Math.h"
#include "GeoPoint.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

TileKey TileKey::INVALID(0, 0, 0, Profile());

TileKey::TileKey(TileKey&& rhs)
{
    *this = std::move(rhs);
}

TileKey& TileKey::operator = (TileKey&& rhs)
{
    _x = rhs._x;
    _y = rhs._y;
    _lod = rhs._lod;
    _profile = std::move(rhs._profile);
    _hash = rhs._hash;
    rhs._profile = {};
    return *this;
}

TileKey::TileKey(
    unsigned int lod, unsigned int tile_x, unsigned int tile_y,
    const Profile& profile)
{
    _x = tile_x;
    _y = tile_y;
    _lod = lod;
    _profile = profile;
    rehash();
}

void
TileKey::rehash()
{
    _hash = valid() ?
        rocky::util::hash_value_unsigned(
            (std::size_t)_lod, 
            (std::size_t)_x,
            (std::size_t)_y,
            _profile.hash()) :
        0ULL;
}

const Profile&
TileKey::profile() const
{
    return _profile;
}

const GeoExtent
TileKey::extent() const
{
    if (!valid())
        return GeoExtent::INVALID;

    auto[width, height] = _profile.tileDimensions(_lod);
    double xmin = _profile.extent().xmin() + (width * (double)_x);
    double ymax = _profile.extent().ymax() - (height * (double)_y);
    double xmax = xmin + width;
    double ymin = ymax - height;

    return GeoExtent( _profile.srs(), xmin, ymin, xmax, ymax );
}

const std::string
TileKey::str() const
{
    if (valid())
    {
        return
            std::to_string(_lod) + '/' +
            std::to_string(_x) + '/' +
            std::to_string(_y);
    }
    else return "invalid";
}

unsigned
TileKey::getQuadrant() const
{
    if ( _lod == 0 )
        return 0;
    bool xeven = (_x & 1) == 0;
    bool yeven = (_y & 1) == 0;
    return 
        xeven && yeven ? 0 :
        xeven          ? 2 :
        yeven          ? 1 : 3;
}

std::pair<double, double>
TileKey::getResolutionForTileSize(unsigned tileSize) const
{
    auto [width, height] = _profile.tileDimensions(_lod);

    return std::make_pair(
        width/(double)(tileSize-1),
        height/(double)(tileSize-1));
}

TileKey
TileKey::createChildKey( unsigned int quadrant ) const
{
    unsigned int lod = _lod + 1;
    unsigned int x = _x * 2;
    unsigned int y = _y * 2;

    if (quadrant == 1)
    {
        x+=1;
    }
    else if (quadrant == 2)
    {
        y+=1;
    }
    else if (quadrant == 3)
    {
        x+=1;
        y+=1;
    }
    return TileKey(lod, x, y, _profile);
}


TileKey
TileKey::createParentKey() const
{
    if (_lod == 0) return TileKey::INVALID;

    unsigned int lod = _lod - 1;
    unsigned int x = _x / 2;
    unsigned int y = _y / 2;
    return TileKey(lod, x, y, _profile);
}

bool
TileKey::makeParent()
{
    if (_lod == 0)
    {
        _profile = Profile(); // invalidate
        return false;
    }

    _lod--;
    _x >>= 1;
    _y >>= 1;
    rehash();
    return true;
}

TileKey
TileKey::createAncestorKey(unsigned ancestorLod) const
{
    if (ancestorLod > _lod)
        return TileKey::INVALID;

    unsigned x = _x, y = _y;
    for (unsigned i = _lod; i > ancestorLod; i--)
    {
        x /= 2;
        y /= 2;
    }
    return TileKey(ancestorLod, x, y, _profile);
}

TileKey
TileKey::createNeighborKey( int xoffset, int yoffset ) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), TileKey::INVALID);

    auto[tx, ty] = profile().numTiles(_lod);

    int sx = (int)_x + xoffset;
    unsigned x =
        sx < 0        ? (unsigned)((int)tx + sx) :
        sx >= (int)tx ? (unsigned)sx - tx :
        (unsigned)sx;

    int sy = (int)_y + yoffset;
    unsigned y =
        sy < 0        ? (unsigned)((int)ty + sy) :
        sy >= (int)ty ? (unsigned)sy - ty :
        (unsigned)sy;

    return TileKey(_lod, x % tx, y % ty, _profile);
}

std::string
TileKey::quadKey() const
{
    std::string buf;
    buf.reserve(_lod + 1);
    for (int i = _lod; i >= 0; i--)
    {
        char digit = '0';
        int mask = 1 << i;
        if ((_x & mask) != 0)
        {
            digit++;
        }
        if ((_y & mask) != 0)
        {
            digit++;
            digit++;
        }
        buf.push_back(digit);
    }
    return buf;
}

TileKey
TileKey::mapResolution(unsigned targetSize,
                       unsigned sourceSize,
                       unsigned minimumLOD) const
{
    // This only works when falling back; i.e. targetSize is smaller than sourceSize.
    if (levelOfDetail() == 0 || targetSize >= sourceSize )
        return *this;

    // Minimum target tile size.
    if ( targetSize < 2 )
        targetSize = 2;

    int lod = (int)levelOfDetail();
    int targetSizePOT = nextPowerOf2((int)targetSize);

    while(true)
    {
        if (targetSizePOT >= (int)sourceSize)
        {
            return createAncestorKey(lod);
        }

        if ( lod == (int)minimumLOD )
        {
            return createAncestorKey(lod);
        }

        lod--;
        targetSizePOT *= 2;
    }
}



TileKey
TileKey::createTileKeyContainingPoint(
    double x, double y, unsigned level,
    const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), TileKey::INVALID);

    auto& extent = profile.extent();

    if (extent.contains(x, y))
    {
        auto [tilesX, tilesY] = profile.numTiles(level);
        //unsigned tilesX = _numTilesWideAtLod0 * (1 << (unsigned)level);
        //unsigned tilesY = _numTilesHighAtLod0 * (1 << (unsigned)level);

        // overflow checks:
#if 0
        if (_numTilesWideAtLod0 == 0u || ((tilesX / _numTilesWideAtLod0) != (1 << (unsigned)level)))
            return TileKey::INVALID;

        if (_numTilesHighAtLod0 == 0u || ((tilesY / _numTilesHighAtLod0) != (1 << (unsigned)level)))
            return TileKey::INVALID;
#endif

        double rx = (x - extent.xmin()) / extent.width();
        int tileX = std::min((unsigned)(rx * (double)tilesX), tilesX - 1);
        double ry = (y - extent.ymin()) / extent.height();
        int tileY = std::min((unsigned)((1.0 - ry) * (double)tilesY), tilesY - 1);

        return TileKey(level, tileX, tileY, profile);
    }
    else
    {
        return TileKey::INVALID;
    }
}

TileKey
TileKey::createTileKeyContainingPoint(
    const GeoPoint& point,
    unsigned level,
    const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(point.valid() && profile.valid(), TileKey::INVALID);

    if (point.srs.horizontallyEquivalentTo(profile.srs()))
    {
        return createTileKeyContainingPoint(
            point.x, point.y, level, profile);
    }
    else
    {
        GeoPoint c;
        point.transform(profile.srs(), c);
        return createTileKeyContainingPoint(c, level, profile);
    }
}

namespace
{
    void addIntersectingKeys(
        const GeoExtent& key_ext,
        unsigned localLOD,
        const Profile& target_profile,
        std::vector<TileKey>& out_intersectingKeys)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(
            !key_ext.crossesAntimeridian(),
            void(),
            "addIntersectingTiles cannot process date-line cross");

        int tileMinX, tileMaxX;
        int tileMinY, tileMaxY;

        auto [destTileWidth, destTileHeight] = target_profile.tileDimensions(localLOD);

        auto profile_extent = target_profile.extent();

        double west = key_ext.xmin() - profile_extent.xmin();
        double east = key_ext.xmax() - profile_extent.xmin();
        double south = profile_extent.ymax() - key_ext.ymin();
        double north = profile_extent.ymax() - key_ext.ymax();

        tileMinX = (int)(west / destTileWidth);
        tileMaxX = (int)(east / destTileWidth);

        tileMinY = (int)(north / destTileHeight);
        tileMaxY = (int)(south / destTileHeight);

        // If the east or west border fell right on a tile boundary
        // but doesn't actually use that tile, detect that and eliminate
        // the extranous tiles. (This happens commonly when mapping
        // geodetic to mercator for example)

        double quantized_west = destTileWidth * (double)tileMinX;
        double quantized_east = destTileWidth * (double)(tileMaxX + 1);

        if (equiv(west - quantized_west, destTileWidth))
            ++tileMinX;
        if (equiv(quantized_east - east, destTileWidth))
            --tileMaxX;

        if (tileMaxX < tileMinX)
            tileMaxX = tileMinX;

        auto[numWide, numHigh] = target_profile.numTiles(localLOD);

        // bail out if the tiles are out of bounds.
        if (tileMinX >= (int)numWide || tileMinY >= (int)numHigh ||
            tileMaxX < 0 || tileMaxY < 0)
        {
            return;
        }

        tileMinX = clamp(tileMinX, 0, (int)numWide - 1);
        tileMaxX = clamp(tileMaxX, 0, (int)numWide - 1);
        tileMinY = clamp(tileMinY, 0, (int)numHigh - 1);
        tileMaxY = clamp(tileMaxY, 0, (int)numHigh - 1);

        for (int i = tileMinX; i <= tileMaxX; ++i)
        {
            for (int j = tileMinY; j <= tileMaxY; ++j)
            {
                //TODO: does not support multi-face destination keys.
                out_intersectingKeys.push_back(TileKey(localLOD, i, j, target_profile));
            }
        }
    }
}

void
TileKey::getIntersectingKeys(
    const Profile& target_profile,
    std::vector<TileKey>& out_intersectingKeys) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), void());

    // If the profiles are exactly equal, just add the given tile key.
    if (profile().horizontallyEquivalentTo(target_profile))
    {
        //Clear the incoming list
        out_intersectingKeys.clear();
        out_intersectingKeys.push_back(*this);
    }
    else
    {
        // figure out which LOD in the local profile is a best match for the LOD
        // in the source LOD in terms of resolution.
        unsigned target_LOD = target_profile.getEquivalentLOD(profile(), levelOfDetail());
        getIntersectingKeys(extent(), target_LOD, target_profile, out_intersectingKeys);
        //ROCKY_DEBUG << LC << "GIT, key=" << key.str() << ", localLOD=" << localLOD
        //    << ", resulted in " << out_intersectingKeys.size() << " tiles" << std::endl;
    }
}

void
TileKey::getIntersectingKeys(
    const GeoExtent& input,
    unsigned localLOD,
    const Profile& target_profile,
    std::vector<TileKey>& out_intersectingKeys)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(input.valid() && target_profile.valid(), void());

    std::vector<GeoExtent> target_extents;
    target_profile.transformAndExtractContiguousExtents(input, target_extents);

    for (auto& extent : target_extents)
    {
        addIntersectingKeys(extent, localLOD, target_profile, out_intersectingKeys);
    }
}
