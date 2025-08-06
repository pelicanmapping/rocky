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

// Scale and bias matrices, one for each TileKey quadrant.
const glm::dmat4 scaleBias[4] =
{
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.5,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.5,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.0,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.0,0,1.0)
};

TileKey TileKey::INVALID(0, 0, 0, Profile());

TileKey::TileKey(TileKey&& rhs) noexcept
{
    *this = std::move(rhs);
}

TileKey& TileKey::operator = (TileKey&& rhs) noexcept
{
    x = rhs.x;
    y = rhs.y;
    level = rhs.level;
    profile = std::move(rhs.profile);
    rhs.profile = {};
    return *this;
}

TileKey::TileKey(unsigned level_, unsigned x_, unsigned y_, const Profile& profile_) :
    x(x_),
    y(y_),
    level(level_),
    profile(profile_)
{
    //NOP
}

const GeoExtent
TileKey::extent() const
{
    if (!valid())
        return GeoExtent::INVALID;

    auto[width, height] = profile.tileDimensions(level);
    double xmin = profile.extent().xmin() + (width * (double)x);
    double ymax = profile.extent().ymax() - (height * (double)y);
    double xmax = xmin + width;
    double ymin = ymax - height;

    return GeoExtent(profile.srs(), xmin, ymin, xmax, ymax);
}

const std::string
TileKey::str() const
{
    if (valid())
    {
        return
            std::to_string(level) + '/' +
            std::to_string(x) + '/' +
            std::to_string(y);
    }
    else return "invalid";
}

unsigned
TileKey::getQuadrant() const
{
    if (level == 0 )
        return 0;
    bool xeven = (x & 1) == 0;
    bool yeven = (y & 1) == 0;
    return 
        xeven && yeven ? 0 :
        xeven          ? 2 :
        yeven          ? 1 : 3;
}

const glm::dmat4
TileKey::scaleBiasMatrix() const
{
    return level > 0 ? scaleBias[getQuadrant()] : glm::dmat4(1);
}

std::pair<double, double>
TileKey::getResolutionForTileSize(unsigned tileSize) const
{
    auto [width, height] = profile.tileDimensions(level);

    return std::make_pair(
        width/(double)(tileSize-1),
        height/(double)(tileSize-1));
}

TileKey
TileKey::createChildKey( unsigned quadrant ) const
{
    unsigned xx = x * 2;
    unsigned yy = y * 2;

    if (quadrant == 1)
    {
        xx += 1;
    }
    else if (quadrant == 2)
    {
        yy += 1;
    }
    else if (quadrant == 3)
    {
        xx += 1;
        yy += 1;
    }
    return TileKey(level + 1, xx, yy, profile);
}


TileKey
TileKey::createParentKey() const
{
    if (level == 0) return TileKey::INVALID;
    return TileKey(level - 1, x / 2, y / 2, profile);
}

bool
TileKey::makeParent()
{
    if (level == 0)
    {
        profile = Profile(); // invalidate
        return false;
    }

    level--;
    x >>= 1;
    y >>= 1;
    return true;
}

TileKey
TileKey::createAncestorKey(unsigned ancestorLod) const
{
    if (ancestorLod > level)
        return TileKey::INVALID;

    unsigned xx = x, yy = y;
    for (unsigned i = level; i > ancestorLod; i--)
    {
        xx /= 2;
        yy /= 2;
    }
    return TileKey(ancestorLod, xx, yy, profile);
}

TileKey
TileKey::createNeighborKey( int xoffset, int yoffset ) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), TileKey::INVALID);

    auto[tx, ty] = profile.numTiles(level);

    int sx = (int)x + xoffset;
    unsigned x =
        sx < 0        ? (unsigned)((int)tx + sx) :
        sx >= (int)tx ? (unsigned)sx - tx :
        (unsigned)sx;

    int sy = (int)y + yoffset;
    unsigned y =
        sy < 0        ? (unsigned)((int)ty + sy) :
        sy >= (int)ty ? (unsigned)sy - ty :
        (unsigned)sy;

    return TileKey(level, x % tx, y % ty, profile);
}

std::string
TileKey::quadKey() const
{
    std::string buf;
    buf.reserve(level + 1);
    for (int i = level; i >= 0; i--)
    {
        char digit = '0';
        int mask = 1 << i;
        if ((x & mask) != 0)
        {
            digit++;
        }
        if ((y & mask) != 0)
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
    if (level == 0 || targetSize >= sourceSize )
        return *this;

    // Minimum target tile size.
    if ( targetSize < 2 )
        targetSize = 2;

    int new_lod = (int)level;
    int targetSizePOT = nextPowerOf2((int)targetSize);

    while(true)
    {
        if (targetSizePOT >= (int)sourceSize)
        {
            return createAncestorKey(new_lod);
        }

        if (new_lod == (int)minimumLOD )
        {
            return createAncestorKey(new_lod);
        }

        new_lod--;
        targetSizePOT *= 2;
    }
}



TileKey
TileKey::createTileKeyContainingPoint(double x, double y, unsigned level, const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), TileKey::INVALID);

    auto& extent = profile.extent();

    if (extent.contains(x, y))
    {
        auto [tilesX, tilesY] = profile.numTiles(level);

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
        GeoPoint c = point.transform(profile.srs());
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

std::vector<TileKey>
TileKey::intersectingKeys(const Profile& target_profile) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), {});

    // If the profiles are exactly equal, just add the given tile key.
    if (profile.horizontallyEquivalentTo(target_profile))
    {
        return { *this };
    }
    else
    {
        // figure out which LOD in the local profile is a best match for the LOD
        // in the source LOD in terms of resolution.
        unsigned target_LOD = target_profile.equivalentLOD(profile, level);
        return intersectingKeys(extent(), target_LOD, target_profile);
    }
}

std::vector<TileKey>
TileKey::intersectingKeys(const GeoExtent& input, unsigned localLOD, const Profile& target_profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(input.valid() && target_profile.valid(), {});

    std::vector<TileKey> output;

    auto target_extents = target_profile.transformAndExtractContiguousExtents(input);

    for (auto& extent : target_extents)
    {
        addIntersectingKeys(extent, localLOD, target_profile, output);
    }

    return output;
}
