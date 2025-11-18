/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TileKey.h"
#include "Math.h"
#include "GeoPoint.h"

using namespace ROCKY_NAMESPACE;

// Scale and bias matrices, one for each TileKey quadrant.
const glm::dmat4 scaleBias[4] =
{
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.5,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.5,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.0,0.0,0,1.0),
    glm::dmat4(0.5,0,0,0, 0,0.5,0,0, 0,0,1.0,0, 0.5,0.0,0,1.0)
};

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

GeoExtent
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
    if (level == 0) return {};
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
        return {};

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
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), {});

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
TileKey::createTileKeyContainingPoint(double x, double y, unsigned level, const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), {});

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
        return {};
    }
}

TileKey
TileKey::createTileKeyContainingPoint(
    const GeoPoint& point,
    unsigned level,
    const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(point.valid() && profile.valid(), {});

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

std::vector<TileKey>
TileKey::intersectingKeys(const Profile& targetProfile) const
{
    // very simple per-thread last-query cache, since in practice this gets
    // called multiple times per thread during terrain creation
    static thread_local struct {
        std::size_t profileHash;
        TileKey key;
        std::vector<TileKey> result;
    } s_previous;

    if (profile == targetProfile)
        return { *this };

    if (*this == s_previous.key && targetProfile.hash() == s_previous.profileHash)
        return s_previous.result;
    
    std::vector<TileKey> output;
    unsigned candidateTiles = 0;

    // convert the source extent to geodetic:
    auto geoSRS = profile.srs().geodeticSRS();
    auto source_geo_ex = extent().transform(geoSRS);

    // convert the target profile to geodetic:
    auto target_geo_ex = targetProfile.geodeticExtent();

    if (source_geo_ex.intersects(target_geo_ex))
    {
        //Log()->info("Tile {} {} get intersecting keys with {}:", str(), profile.wellKnownName(), targetProfile.wellKnownName());

        // create a span in each dimension for resolution-matching.
        auto source_geo_c = source_geo_ex.centroid();
        double dlon = source_geo_ex.width() - 1e-10;
        double dlat = source_geo_ex.height() - 1e-10;

        unsigned target_lod = 0;
        auto geo_to_target = geoSRS.to(targetProfile.srs());

        // trivial rejection if the source extent is larger than the entire target extent.
        if (target_geo_ex.width() > dlon || target_geo_ex.height() > dlat)
        {
            //what about out-of-bounds errors in the xform?

            auto geo_to_target = geoSRS.to(targetProfile.srs());

            auto dxb = glm::length(
                geo_to_target(glm::dvec3{ source_geo_c.x + dlon / 2.0, source_geo_c.y, 0 }) -
                geo_to_target(glm::dvec3{ source_geo_c.x - dlon / 2.0, source_geo_c.y, 0 }));

            auto dyb = glm::length(
                geo_to_target(glm::dvec3{ source_geo_c.x, std::clamp(source_geo_c.y + dlat / 2.0, -90.0, 90.0), 0 }) -
                geo_to_target(glm::dvec3{ source_geo_c.x, std::clamp(source_geo_c.y - dlat / 2.0, -90.0, 90.0), 0 }));

            // Find the level of detail with the smallest error relative to our span size:
            auto dims0 = targetProfile.tileDimensions(0);

            double xe = std::abs(std::log2(dims0.x / std::max(dxb, 1e-12)));
            double ye = std::abs(std::log2(dims0.y / std::max(dyb, 1e-12)));

            if (targetProfile.srs().isGeodetic())
            {
                // for geodetic only use the Y-axis error.
                target_lod = (unsigned)std::llround(ye);
            }
            else
            {
                // for a projected profile, also check the x-axis error and average them:
                target_lod = (unsigned)std::llround((xe + ye) * 0.5);
            }
            //Log()->info("  Target LOD = {} (dlon={} dlat={}) (dxb={}, dyb={}) (ex={} ey={})", target_lod, dlon, dlat, dxb, dyb, xe, ye);

            target_lod = std::min(target_lod, 30u);
        }

        // collect the candidate tiles.
        auto target_profile_ex = targetProfile.extent();
        auto target_ex = source_geo_ex.transform(targetProfile.srs());
        auto dims = targetProfile.tileDimensions(target_lod);
        auto tiles = targetProfile.numTiles(target_lod);

        int colmin = std::clamp((int)std::floor((target_ex.xmin() - target_profile_ex.xmin()) / dims.x), 0, (int)tiles.x - 1);
        int colmax = std::clamp((int)std::floor((target_ex.xmax() - target_profile_ex.xmin()) / dims.x), 0, (int)tiles.x - 1);
        int rowmin = std::clamp((int)std::floor((target_profile_ex.ymax() - target_ex.ymax()) / dims.y), 0, (int)tiles.y - 1);
        int rowmax = std::clamp((int)std::floor((target_profile_ex.ymax() - target_ex.ymin()) / dims.y), 0, (int)tiles.y - 1);

        candidateTiles = (colmax - colmin + 1) * (rowmax - rowmin + 1);

        // todo: intersect test the boundary against the candidate tiles.
        for (unsigned col = colmin; col <= (unsigned)colmax; ++col)
        {
            for (unsigned row = rowmin; row <= (unsigned)rowmax; ++row)
            {
                TileKey ikey(target_lod, col, row, targetProfile);

                if (ikey.valid())
                {
                    // Note: a densified boundary polygon would be more accurate, but this is fine for now
                    if (target_ex.intersects(ikey.extent()))
                    {
                        output.emplace_back(ikey);
                    }
                }
            }
        }
    }

    s_previous.key = *this;
    s_previous.profileHash = targetProfile.hash();
    s_previous.result = output;

    return output;
}
