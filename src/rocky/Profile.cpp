/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Profile.h"
#include "TileKey.h"
#include "Math.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

#define LC "[Profile] "


//Definitions for the mercator extent
const double MERC_MINX = -20037508.34278925;
const double MERC_MINY = -20037508.34278925;
const double MERC_MAXX = 20037508.34278925;
const double MERC_MAXY = 20037508.34278925;
const double MERC_WIDTH = MERC_MAXX - MERC_MINX;
const double MERC_HEIGHT = MERC_MAXY - MERC_MINY;

void
Profile::setup(
    const SRS& srs,
    const Box& bounds,
    unsigned numTilesWideAtLod0,
    unsigned numTilesHighAtLod0)
{
    if (srs.valid())
    {
        Box b;
        unsigned tx = numTilesWideAtLod0;
        unsigned ty = numTilesHighAtLod0;

        if (bounds.valid())
        {
            b = bounds;
        }
        else
        {
            b = srs.bounds();

            //TODO: CHECK THIS
            ROCKY_TODO("");

#if 0
            if (srs.isGeodetic())
            {
                b = Box(-180.0, -90.0, 180.0, 90.0);
            }

            else if (srs.isMercator())
            {
                // automatically figure out proper mercator extents:
                dvec3 point(180.0, 0.0, 0.0);
                srs.getGeographicSRS()->transform(point, srs.get(), point);
                double e = point.x;
                b = Box(-e, -e, e, e);
            }
            else
            {
                ROCKY_INFO << LC << "No extents given, making some up.\n";
                b = srs.bounds();
            }
#endif
        }

        if (tx == 0 || ty == 0)
        {
            ROCKY_TODO("");
#if 0
            if (srs.isGeodetic())
            {
                tx = 2;
                ty = 1;
            }
            else if (srs.isMercator())
            {
                tx = 1;
                ty = 1;
            }
            else
#endif
                if (b.valid())
            {
                double ar = b.width() / b.height();
                if (ar >= 1.0) {
                    int ari = (int)ar;
                    tx = ari;
                    ty = 1;
                }
                else {
                    int ari = (int)(1.0 / ar);
                    tx = 1;
                    ty = ari;
                }
            }
            else // default.
            {
                tx = 1;
                ty = 1;
            }
        }

        _shared->_extent = GeoExtent(srs, b);

        _shared->_numTilesWideAtLod0 = tx;
        _shared->_numTilesHighAtLod0 = ty;

        // automatically calculate the lat/long extents:
        _shared->_latlong_extent = srs.isGeodetic() ?
            _shared->_extent :
            _shared->_extent.transform(srs.geoSRS());

        // make a profile sig (sans srs) and an srs sig for quick comparisons.
        JSON temp = to_json();
        _shared->_fullSignature = util::Stringify() << std::hex << util::hashString(temp); // .to_json());
        //temp.object.erase("vdatum");
        //temp.vsrsString() = "";
        //_shared->_horizSignature = util::Stringify() << std::hex << util::hashString(temp); // .to_json());

        _shared->_hash = std::hash<std::string>()(temp); // .to_json());
    }
}

// invalid profile
Profile::Profile()
{
    _shared = std::make_shared<Data>();
}

Profile&
Profile::operator=(Profile&& rhs)
{
    *this = (const Profile&)rhs;
    rhs._shared = nullptr;
    return *this;
}

bool
Profile::operator == (const Profile& rhs) const
{
    if (!valid() || !rhs.valid())
        return false;

    if (_shared == rhs._shared)
        return true;

    if (_shared->_wellKnownName == rhs._shared->_wellKnownName)
        return true;

    return
        _shared->_extent == rhs._shared->_extent &&
        _shared->_numTilesWideAtLod0 == rhs._shared->_numTilesHighAtLod0;
}


Profile::Profile(const std::string& wellKnownName)
{
    _shared = std::make_shared<Data>();
    setup(wellKnownName);
}

Profile::Profile(
    const SRS& srs,
    const Box& bounds,
    unsigned x_tiles_at_lod0,
    unsigned y_tiles_at_lod0)
{
    _shared = std::make_shared<Data>();
    setup(srs, bounds, x_tiles_at_lod0, y_tiles_at_lod0);
}

void
Profile::setup(const std::string& name)
{
    _shared->_wellKnownName = name;

    if (util::ciEquals(name, "plate-carree") ||
        util::ciEquals(name, "plate-carre") ||
        util::ciEquals(name, "eqc-wgs84"))
    {
        // Yes I know this is not really Plate Carre but it will stand in for now.
        dvec3 ex;

        SRS::WGS84.to(SRS::PLATE_CARREE).transform(dvec3(180, 90, 0), ex);

        setup(
            SRS::PLATE_CARREE,
            Box(-ex.x, -ex.y, ex.x, ex.y),
            2u, 1u);
    }
    else if (util::ciEquals(name, "global-geodetic"))
    {
        setup(
            SRS::WGS84,
            Box(-180.0, -90.0, 180.0, 90.0),
            2, 1);
    }
    else if (util::ciEquals(name, "spherical-mercator"))
    {
        setup(
            SRS::SPHERICAL_MERCATOR,
            Box(MERC_MINX, MERC_MINY, MERC_MAXX, MERC_MAXY),
            1, 1);
    }
}

bool
Profile::valid() const {
    return _shared->_extent.valid();
}

const SRS&
Profile::srs() const {
    return _shared->_extent.srs();
}

const GeoExtent&
Profile::extent() const {
    return _shared->_extent;
}

const GeoExtent&
Profile::geographicExtent() const {
    return _shared->_latlong_extent;
}

const std::string&
Profile::wellKnownName() const {
    return _shared->_wellKnownName;
}

JSON
Profile::to_json() const
{
    json temp;
    ROCKY_NAMESPACE::to_json(temp, *this);
    return temp.dump();
}

Profile
Profile::overrideSRS(const SRS& srs) const
{
    return Profile(
        srs,
        Box(_shared->_extent.xMin(), _shared->_extent.yMin(), _shared->_extent.xMax(), _shared->_extent.yMax()),
        _shared->_numTilesWideAtLod0, _shared->_numTilesHighAtLod0);
}

void
Profile::getRootKeys(
    const Profile& profile,
    std::vector<TileKey>& out_keys)
{
    getAllKeysAtLOD(0, profile, out_keys);
}

void
Profile::getAllKeysAtLOD(
    unsigned lod,
    const Profile& profile,
    std::vector<TileKey>& out_keys)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), void());

    out_keys.clear();

    auto[tx, ty] = profile.numTiles(lod);

    for (unsigned c = 0; c < tx; ++c)
    {
        for (unsigned r = 0; r < ty; ++r)
        {
            out_keys.push_back(TileKey(lod, c, r, profile));
        }
    }
}

GeoExtent
Profile::tileExtent(unsigned lod, unsigned tileX, unsigned tileY) const
{
    auto [width, height] = tileDimensions(lod);

    double xmin = extent().xMin() + (width * (double)tileX);
    double ymax = extent().yMax() - (height * (double)tileY);
    double xmax = xmin + width;
    double ymin = ymax - height;

    return GeoExtent(srs(), xmin, ymin, xmax, ymax);
}

std::pair<double,double>
Profile::tileDimensions(unsigned int lod) const
{
    double out_width  = _shared->_extent.width() / (double)_shared->_numTilesWideAtLod0;
    double out_height = _shared->_extent.height() / (double)_shared->_numTilesHighAtLod0;

    double factor = double(1u << lod);
    out_width /= (double)factor;
    out_height /= (double)factor;

    return std::make_pair(out_width, out_height);
}

std::pair<unsigned, unsigned>
Profile::numTiles(unsigned lod) const
{
    unsigned out_tiles_wide = _shared->_numTilesWideAtLod0;
    unsigned out_tiles_high = _shared->_numTilesHighAtLod0;

    double factor = double(1u << lod);
    out_tiles_wide *= factor;
    out_tiles_high *= factor;

    return std::make_pair(out_tiles_wide, out_tiles_high);
}

unsigned int
Profile::getLevelOfDetailForHorizResolution( double resolution, int tileSize ) const
{
    if ( tileSize <= 0 || resolution <= 0.0 ) return 23;

    double tileRes = (_shared->_extent.width() / (double)_shared->_numTilesWideAtLod0) / (double)tileSize;
    unsigned int level = 0;
    while( tileRes > resolution ) 
    {
        level++;
        tileRes *= 0.5;
    }
    return level;
}

GeoExtent
Profile::clampAndTransformExtent(const GeoExtent& input, bool* out_clamped) const
{
    // initialize the output flag
    if ( out_clamped )
        *out_clamped = false;

    // null checks
    if (!input.valid())
        return GeoExtent::INVALID;

    if (input.isWholeEarth())
    {
        if (out_clamped && !extent().isWholeEarth())
            *out_clamped = true;
        return extent();
    }

    // begin by transforming the input extent to this profile's SRS.
    GeoExtent inputInMySRS = input.transform(srs());

    if (inputInMySRS.valid())
    {
        // Compute the intersection of the two:
        GeoExtent intersection = inputInMySRS.intersectionSameSRS(extent());

        // expose whether clamping took place:
        if (out_clamped != 0L)
        {
            *out_clamped = intersection != extent();
        }

        return intersection;
    }

    else
    {
        // The extent transformation failed, probably due to an out-of-bounds condition.
        // Go to Plan B: attempt the operation in lat/long
        auto geo_srs = srs().geoSRS();

        // get the input in lat/long:
        GeoExtent gcs_input =
            input.srs().isGeodetic() ?
            input :
            input.transform(geo_srs);

        // bail out on a bad transform:
        if (!gcs_input.valid())
            return GeoExtent::INVALID;

        // bail out if the extent's do not intersect at all:
        if (!gcs_input.intersects(geographicExtent(), false))
            return GeoExtent::INVALID;

        // clamp it to the profile's extents:
        GeoExtent clamped_gcs_input = GeoExtent(
            gcs_input.srs(),
            clamp(gcs_input.xMin(), geographicExtent().xMin(), geographicExtent().xMax()),
            clamp(gcs_input.yMin(), geographicExtent().yMin(), geographicExtent().yMax()),
            clamp(gcs_input.xMax(), geographicExtent().xMin(), geographicExtent().xMax()),
            clamp(gcs_input.yMax(), geographicExtent().yMin(), geographicExtent().yMax()));

        if (out_clamped)
            *out_clamped = (clamped_gcs_input != gcs_input);

        // finally, transform the clamped extent into this profile's SRS and return it.
        GeoExtent result =
            clamped_gcs_input.srs() == srs() ?
            clamped_gcs_input :
            clamped_gcs_input.transform(srs());

        if (!result.valid())
        {
            Log::warn() << "Oh no." << std::endl;
            //return clampAndTransformExtent(input, out_clamped);
        }

        return result;
    }    
}

unsigned
Profile::getEquivalentLOD(const Profile& rhsProfile, unsigned rhsLOD) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(rhsProfile.valid(), rhsLOD);

    //If the profiles are equivalent, just use the incoming lod
    if (*this == rhsProfile)
        return rhsLOD;

    // Special check for geodetic to mercator or vise versa, they should match up in LOD.
    // TODO not sure about this.. -gw
    if (((rhsProfile == Profile::SPHERICAL_MERCATOR) && (*this == Profile::GLOBAL_GEODETIC)) ||
        ((rhsProfile == Profile::GLOBAL_GEODETIC) && (*this == Profile::SPHERICAL_MERCATOR)))
    {
        return rhsLOD;
    }

    auto[rhsWidth, rhsHeight] = rhsProfile.tileDimensions(rhsLOD);

    // safety catch
    if (equiv(rhsWidth, 0.0) || equiv(rhsHeight, 0.0))
    {
        std::cerr << LC << "[rk] Profile: getEquivalentLOD: zero dimension" << std::endl;
        return rhsLOD;
    }

    double rhsTargetHeight = SRS::transformUnits(
        rhsHeight, rhsProfile.srs(), srs(), Angle());

    int currLOD = 0;
    int destLOD = currLOD;

    double delta = DBL_MAX;

    // Find the LOD that most closely matches the resolution of the incoming key.
    // We use the closest (under or over) so that you can match back and forth between profiles and be sure to get the same results each time.
    while (true)
    {
        double prevDelta = delta;

        auto[w, h] = tileDimensions(currLOD);

        delta = fabs(h - rhsTargetHeight);
        if (delta < prevDelta)
        {
            // We're getting closer so keep going
            destLOD = currLOD;
        }
        else
        {
            // We are further away from the previous lod so stop.
            break;
        }
        currLOD++;
    }
    return destLOD;
}

unsigned
Profile::levelOfDetail(double height) const
{
    int currLOD = 0;
    int destLOD = currLOD;

    double delta = DBL_MAX;

    // Find the LOD that most closely matches the target height in this profile.
    while (true)
    {
        double prevDelta = delta;

        auto[w, h] = tileDimensions(currLOD);

        delta = fabs(h - height);
        if (delta < prevDelta)
        {
            // We're getting closer so keep going
            destLOD = currLOD;
        }
        else
        {
            // We are further away from the previous lod so stop.
            break;
        }
        currLOD++;
    }
    return destLOD;
}

bool
Profile::transformAndExtractContiguousExtents(
    const GeoExtent& input,
    std::vector<GeoExtent>& output) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid() && input.valid(), false);

    GeoExtent target_extent = input;

    // reproject into the profile's SRS if necessary:
    if (!srs().isHorizEquivalentTo(input.srs()))
    {
        // localize the extents and clamp them to legal values
        target_extent = clampAndTransformExtent(input);
        if (!target_extent.valid())
            return false;
    }

    if (target_extent.crossesAntimeridian())
    {
        GeoExtent first, second;
        if (target_extent.splitAcrossAntimeridian(first, second))
        {
            output.push_back(first);
            output.push_back(second);
        }
    }
    else
    {
        output.push_back(target_extent);
    }

    return true;
}


#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const Profile& obj)
    {
        if (obj.valid())
        {
            if (!obj.wellKnownName().empty())
            {
                j = obj.wellKnownName(); // string
            }
            else
            {
                j = json::object();
                set(j, "extent", obj.extent());
                auto [tx, ty] = obj.numTiles(0);
                set(j, "tx", tx);
                set(j, "ty", ty);
            }
        }
        else j = nullptr;
    }

    void from_json(const json& j, Profile& obj)
    {
        if (j.is_string())
        {
            obj = Profile(get_string(j));
        }
        else if (j.is_object())
        {
            GeoExtent extent;
            unsigned tx = 0, ty = 0;

            get_to(j, "extent", extent);
            get_to(j, "tx", tx);
            get_to(j, "ty", ty);

            if (extent.valid())
            {
                obj = Profile(extent.srs(), extent.bounds(), tx, ty);
            }
        }
        else
        {
            obj = Profile();
        }
    }
}

