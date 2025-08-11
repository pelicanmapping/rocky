/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileKey.h"
#include "Math.h"
#include "GeoPoint.h"
#include <array>

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

#if 0
std::vector<TileKey>
TileKey::intersectingKeys(const Profile& target_profile) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), {});

    // If the profiles are exactly equal, just add the given tile key.
    if (profile == target_profile)
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

#else

namespace
{
    template<typename ITER>
    inline void normalizeLongitudes(ITER begin, ITER end, double lonRef) {

        if (begin == end) return;
        double L0 = begin->x;
        double K0 = std::round((lonRef - L0) / 360.0);
        double prev = L0 + K0 * 360.0;
        begin->x = prev;
        begin->y = clamp(begin->y, -90.0, 90.0);
        begin++;
        while (begin != end)
        {
            double Li = begin->x;
            while (Li - prev > 180.0) Li -= 360.0;
            while (prev - Li > 180.0) Li += 360.0;
            begin->x = Li;
            begin->y = clamp(begin->y, -90.0, 90.0);
            prev = Li;
            begin++;
        }
    }
}

std::vector<TileKey>
TileKey::intersectingKeys(const Profile& target_profile, unsigned tile_size) const
{
    if (profile == target_profile)
        return { *this };

    //Log()->info("Tile {} {} get intersecting keys with {}:", str(), profile.wellKnownName(), target_profile.wellKnownName());

    auto source_extent = extent();
    auto& source_srs = profile.srs();
    auto& geo = source_srs.geodeticSRS();

    // create a (possibly densified) boundary polygon.
    // The epsilon keeps up from overrunning lat/long extrema:
    double E = source_srs.isProjected() ? 0.5 : 1e-5;

    std::array<glm::dvec3, 4> corners = {
        glm::dvec3(source_extent.xmin() + E, source_extent.ymin() + E, 0),
        glm::dvec3(source_extent.xmax() - E, source_extent.ymin() + E, 0),
        glm::dvec3(source_extent.xmax() - E, source_extent.ymax() - E, 0),
        glm::dvec3(source_extent.xmin() + E, source_extent.ymax() - E, 0)
    };

#if 0
    // densification:
    std::vector<glm::dvec3> boundary;
    boundary.reserve(corners.size() * 6);
    for (int i = 0; i < corners.size()-1; ++i)
    {
        for (double t = 0.0; t < 1.0; t += 1.0 / 6.0)
        {
            boundary.emplace_back(GeoPoint(source_srs, corners[i]).interpolateTo(GeoPoint(source_srs, corners[i + 1]), t));
        }
    }
#else
    auto boundary = corners;
#endif

    // convert to geodetic.
    auto source_to_geo = source_srs.to(geo);
    auto source_centroid = (corners[0] + corners[2]) * 0.5;
    auto geo_centroid = source_to_geo(source_centroid);

    source_to_geo.transformRange(boundary.begin(), boundary.end());
    normalizeLongitudes(boundary.begin(), boundary.end(), geo_centroid.x);

    // calculate the geodetic bounding box.
    Box box;
    box.expandBy(boundary.begin(), boundary.end());

    // use it as our new boundary.
    boundary = {
        glm::dvec3(box.xmin, box.ymin, 0),
        glm::dvec3(box.xmax, box.ymin, 0),
        glm::dvec3(box.xmax, box.ymax, 0),
        glm::dvec3(box.xmin, box.ymax, 0)
    };

    // create a local span in each dimension for resolution matching.
    double dlon = box.width();
    double dlat = box.height();

    // project the local spans into the target SRS.
    auto geo_to_target = geo.to(target_profile.srs());

    auto dxb = glm::length(
        geo_to_target(glm::dvec3{ geo_centroid.x + dlon / 2.0, geo_centroid.y, 0 }) -
        geo_to_target(glm::dvec3{ geo_centroid.x - dlon / 2.0, geo_centroid.y, 0 }));

    auto dyb = glm::length(
        geo_to_target(glm::dvec3{ geo_centroid.x, clamp(geo_centroid.y + dlat / 2.0, -90.0, 90.0), 0 }) -
        geo_to_target(glm::dvec3{ geo_centroid.x, clamp(geo_centroid.y - dlat / 2.0, -90.0, 90.0), 0 }));

    // calculate the target LOD
    auto dims0 = target_profile.tileDimensions(0);
    double ex = std::abs(std::log2(dims0.x / std::max(dxb, 1e-12)));
    double ey = std::abs(std::log2(dims0.y / std::max(dyb, 1e-12)));

    // for a geodetic target, we only care about the Y-axis error
    unsigned target_lod =
        target_profile.srs().isGeodetic() ? (unsigned)std::llround(ey) :
        (unsigned)std::llround((ex + ey) * 0.5);
        
    target_lod = std::min(target_lod, 30u);
    
    //Log()->info("  Target LOD = {} (dlon={} dlat={}) (dxb={}, dyb={}) (ex={} ey={})", target_lod, dlon, dlat, dxb, dyb, ex, ey);
    //Log()->info("  Geodetic centroid = {} {}", geo_centroid.x, geo_centroid.y);

    // boundary into target coords:
    geo_to_target.transformRange(boundary.begin(), boundary.end());

    // collect the candidate tiles.
    auto target_ex = target_profile.extent();
    auto dims = target_profile.tileDimensions(target_lod);

#if 1
    Box targetBox;
    targetBox.expandBy(boundary.begin(), boundary.end());

    int colmin = (int)std::floor((targetBox.xmin - target_ex.xmin()) / dims.x);
    int colmax = (int)std::floor((targetBox.xmax - target_ex.xmin()) / dims.x);
    int rowmin = (int)std::floor((target_ex.ymax() - targetBox.ymax) / dims.y);
    int rowmax = (int)std::floor((target_ex.ymax() - targetBox.ymin) / dims.y);
#else
    int colmin = INT_MAX, colmax = 0, rowmin = INT_MAX, rowmax = 0;
    for (auto& p : boundary) // boundary is in the target SRS
    {
        double col = (p.x - target_ex.xmin()) / dims.x;
        double row = (target_ex.ymax() - p.y) / dims.y;

        colmin = std::min(colmin, (int)std::floor(col));
        colmax = std::max(colmax, (int)std::floor(col));
        rowmin = std::min(rowmin, (int)std::floor(row));
        rowmax = std::max(rowmax, (int)std::floor(row));
    }
#endif
    colmin = clamp(colmin - 1, 0, (int)target_profile.numTiles(target_lod).x - 1);
    colmax = clamp(colmax + 1, 0, (int)target_profile.numTiles(target_lod).x - 1);
    rowmin = clamp(rowmin - 1, 0, (int)target_profile.numTiles(target_lod).y - 1);
    rowmax = clamp(rowmax + 1, 0, (int)target_profile.numTiles(target_lod).y - 1);

    std::vector<TileKey> output;
    auto tiles = target_profile.numTiles(target_lod);
    std::array<glm::dvec3, 4> tile_box;

    // todo: intersect test the boundary against the candidate tiles.
    for (unsigned col = colmin; col <= colmax; ++col)
    {
        for (unsigned row = rowmin; row <= rowmax; ++row)
        {
            TileKey ikey(target_lod, col, row, target_profile);

            if (ikey.valid())
            {
                auto tile_ex = ikey.extent();
                tile_box[0] = glm::dvec3(tile_ex.xmin(), tile_ex.ymin(), 0);
                tile_box[1] = glm::dvec3(tile_ex.xmax(), tile_ex.ymin(), 0);
                tile_box[2] = glm::dvec3(tile_ex.xmax(), tile_ex.ymax(), 0);
                tile_box[3] = glm::dvec3(tile_ex.xmin(), tile_ex.ymax(), 0);

                if (util::polygonsIntersect2D(boundary.begin(), boundary.end(), tile_box.begin(), tile_box.end()))
                {
                    output.emplace_back(ikey);
                }
            }
        }
    }

    //Log()->info("  done - found {}/{} keys", output.size(), (colmax-colmin+1)*(rowmax-rowmin+1));

    return output;
}

#endif