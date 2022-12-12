/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Profile.h"
#include "TileKey.h"
#include "Math.h"
#include "VerticalDatum.h"
#include "Notify.h"

using namespace rocky;

#define LC "[Profile] "

#if 0
//------------------------------------------------------------------------

ProfileOptions::ProfileOptions(const ConfigOptions& options) :
    ConfigOptions(options)
{
    numTilesWideAtLod0().setDefault(0);
    numTilesHighAtLod0().setDefault(0);
    fromConfig(_conf);
}

ProfileOptions::ProfileOptions(const std::string& namedProfile)
{
    numTilesWideAtLod0().setDefault(0);
    numTilesHighAtLod0().setDefault(0);
    _namedProfile = namedProfile; // don't set above
}

void
ProfileOptions::mergeConfig(const Config& conf) {
    ConfigOptions::mergeConfig(conf);
    fromConfig(conf);
}

void
ProfileOptions::fromConfig(const Config& conf)
{
    if ( !conf.value().empty() )
        _namedProfile = conf.value();

    if (conf.hasChild("profile"))
        fromConfig(conf.child("profile"));

    conf.get( "srs", srsString() );
    conf.get( "vdatum", vsrsString() );
    conf.get( "vsrs", vsrsString()); // back compat

    if ( conf.hasValue( "xmin" ) && conf.hasValue( "ymin" ) && conf.hasValue( "xmax" ) && conf.hasValue( "ymax" ) )
    {
        _bounds = Box(
            conf.value<double>("xmin", 0),
            conf.value<double>("ymin", 0),
            0.0,
            conf.value<double>("xmax", 0),
            conf.value<double>("ymax", 0),
            0.0);
    }

    conf.get( "num_tiles_wide_at_lod_0", _numTilesWideAtLod0 );
    conf.get( "tx", _numTilesWideAtLod0 );
    conf.get( "num_tiles_high_at_lod_0", _numTilesHighAtLod0 );
    conf.get( "ty", _numTilesHighAtLod0 );
}

Config
ProfileOptions::getConfig() const
{
    Config conf( "profile" );
    if (namedProfile().has_value() )
    {
        conf.setValue(namedProfile().value());
    }
    else
    {
        conf.set( "srs", srsString());
        conf.set( "vdatum", vsrsString() );

        if ( _bounds.has_value() )
        {
            conf.set( "xmin", std::to_string(_bounds->xmin) );
            conf.set( "ymin", std::to_string(_bounds->ymin) );
            conf.set( "xmax", std::to_string(_bounds->xmax) );
            conf.set( "ymax", std::to_string(_bounds->ymax) );
        }

        conf.set( "num_tiles_wide_at_lod_0", _numTilesWideAtLod0 );
        conf.set( "num_tiles_high_at_lod_0", _numTilesHighAtLod0 );
    }
    return conf;
}

bool
ProfileOptions::defined() const
{
    return namedProfile().has_value() || srsString().has_value();
}
#endif

/***********************************************************************/

const std::string Profile::GLOBAL_GEODETIC("global-geodetic");
const std::string Profile::GLOBAL_MERCATOR("global-mercator");
const std::string Profile::SPHERICAL_MERCATOR("spherical-mercator");
const std::string Profile::PLATE_CARREE("plate-carree");

void
Profile::setup(
    shared_ptr<SRS> srs,
    const Box& bounds,
    unsigned numTilesWideAtLod0,
    unsigned numTilesHighAtLod0)
{
    if (srs)
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
            if (srs->isGeographic())
            {
                b = Box(-180.0, -90.0, 180.0, 90.0);
            }
            else if (srs->isMercator())
            {
                // automatically figure out proper mercator extents:
                dvec3 point(180.0, 0.0, 0.0);
                srs->getGeographicSRS()->transform(point, srs.get(), point);
                double e = point.x;
                b = Box(-e, -e, e, e);
            }
            else
            {
                ROCKY_INFO << LC << "No extents given, making some up.\n";
                b = srs->getBounds();
            }
        }

        if (tx == 0 || ty == 0)
        {
            if (srs->isGeographic())
            {
                tx = 2;
                ty = 1;
            }
            else if (srs->isMercator())
            {
                tx = 1;
                ty = 1;
            }
            else if (b.valid())
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

        _extent = GeoExtent(srs, b);

        _numTilesWideAtLod0 = tx;
        _numTilesHighAtLod0 = ty;

        // automatically calculate the lat/long extents:
        _latlong_extent = srs->isGeographic() ?
            _extent :
            _extent.transform(srs->getGeographicSRS());

        // make a profile sig (sans srs) and an srs sig for quick comparisons.
        Config temp = getConfig();
        _fullSignature = util::Stringify() << std::hex << util::hashString(temp.toJSON());
        temp.remove("vdatum");
        //temp.vsrsString() = "";
        _horizSignature = util::Stringify() << std::hex << util::hashString(temp.toJSON());

        _hash = std::hash<std::string>()(temp.toJSON());
    }
}

Profile::Profile(const Config& conf)
{
    //Config input = conf;

    if (!conf.value().empty())
    {
        setup(conf.value());
    }
    else
    {
        //if (input.hasChild("profile"))
        //    input = conf.child("profile");

        std::string horiz, vdatum;
        Box box;
        unsigned tx = 0, ty = 0;

        conf.get("srs", horiz);
        conf.get("vdatum", vdatum);

        box = Box(
            conf.value<double>("xmin", 0),
            conf.value<double>("ymin", 0),
            conf.value<double>("xmax", 0),
            conf.value<double>("ymax", 0));

        conf.get("num_tiles_wide_at_lod_0", tx);
        conf.get("tx", tx);
        conf.get("num_tiles_high_at_lod_0", ty);
        conf.get("ty", ty);

        auto srs = SRS::get(horiz, vdatum);

        if (srs)
        {
            setup(srs, box, tx, ty);
        }
    }
}

Config
Profile::getConfig() const
{
    Config conf("profile");

    if (valid())
    {
        if (_wellKnownName.empty() == false)
        {
            conf.setValue(_wellKnownName);
        }
        else
        {
            conf.set("srs", getSRS()->getHorizInitString());
            if (!getSRS()->getVertInitString().empty())
                conf.set("vdatum", getSRS()->getVertInitString());
            conf.set("xmin", _extent.xmin());
            conf.set("ymin", _extent.ymin());
            conf.set("xmax", _extent.xmax());
            conf.set("ymax", _extent.ymax());
            conf.set("tx", _numTilesWideAtLod0);
            conf.set("ty", _numTilesHighAtLod0);
        }
    }

    return conf;
}

Profile::Profile(const std::string& wellKnownName)
{
    setup(wellKnownName);
}

Profile::Profile(
    shared_ptr<SRS> srs,
    const Box& bounds,
    unsigned x_tiles_at_lod0,
    unsigned y_tiles_at_lod0)
{
    setup(srs, bounds, x_tiles_at_lod0, y_tiles_at_lod0);
}

void
Profile::setup(const std::string& name)
{
    _wellKnownName = name;

    if (util::ciEquals(name, PLATE_CARREE) ||
        util::ciEquals(name, "plate-carre") ||
        util::ciEquals(name, "eqc-wgs84"))
    {
        // Yes I know this is not really Plate Carre but it will stand in for now.
        dvec3 ex;
        auto plateCarre = SRS::get("plate-carre");
        auto wgs84 = SRS::get("wgs84");
        wgs84->transform(dvec3(180,90,0), plateCarre.get(), ex);

        setup(
            plateCarre,
            Box(-ex.x, -ex.y, ex.x, ex.y),
            2u, 1u);
    }
    else if (util::ciEquals(name, GLOBAL_GEODETIC))
    {
        setup(
            SRS::get("wgs84"),
            Box(-180.0, -90.0, 180.0, 90.0),
            2, 1);
    }
    else if (util::ciEquals(name, GLOBAL_MERCATOR))
    {
        setup(
            SRS::get("global-mercator"),
            Box(MERC_MINX, MERC_MINY, MERC_MAXX, MERC_MAXY),
            1, 1);
    }
    else if (util::ciEquals(name, SPHERICAL_MERCATOR))
    {
        setup(
            SRS::get("spherical-mercator"),
            Box(MERC_MINX, MERC_MINY, MERC_MAXX, MERC_MAXY),
            1, 1);
    }
}

bool
Profile::valid() const {
    return _extent.valid();
}

shared_ptr<SRS>
Profile::getSRS() const {
    return _extent.getSRS();
}

const GeoExtent&
Profile::getExtent() const {
    return _extent;
}

const GeoExtent&
Profile::getLatLongExtent() const {
    return _latlong_extent;
}

std::string
Profile::toString() const
{
    auto srs = _extent.getSRS();
    return util::make_string()
        << std::setprecision(16)
        << "[srs=" << srs->getName() << ", min=" << _extent.xMin() << "," << _extent.yMin()
        << " max=" << _extent.xMax() << "," << _extent.yMax()
        << " ar=" << _numTilesWideAtLod0 << ":" << _numTilesHighAtLod0
        << " vdatum=" << (srs->getVerticalDatum() ? srs->getVerticalDatum()->getName() : "geodetic")
        << "]";
}

#if 0
ProfileOptions
Profile::toProfileOptions() const
{
    ProfileOptions op;
    if (_wellKnownName.empty() == false)
    {
        op.namedProfile() = _wellKnownName;
    }
    else
    {
        op.srsString() = getSRS()->getHorizInitString();
        op.vsrsString() = getSRS()->getVertInitString();
        op.bounds()->xmin = _extent.xMin();
        op.bounds()->ymin = _extent.yMin();
        op.bounds()->xmax = _extent.xMax();
        op.bounds()->ymax = _extent.yMax();
        op.numTilesWideAtLod0() = _numTilesWideAtLod0;
        op.numTilesHighAtLod0() = _numTilesHighAtLod0;
    }
    return op;
}
#endif

Profile::ptr
Profile::overrideSRS(shared_ptr<SRS> srs) const
{
    return Profile::create(
        srs,
        Box(_extent.xMin(), _extent.yMin(), _extent.xMax(), _extent.yMax()),
        _numTilesWideAtLod0, _numTilesHighAtLod0);
}

void
Profile::getRootKeys(
    shared_ptr<Profile> profile,
    std::vector<TileKey>& out_keys)
{
    getAllKeysAtLOD(0, profile, out_keys);
}

void
Profile::getAllKeysAtLOD(
    unsigned lod,
    shared_ptr<Profile> profile,
    std::vector<TileKey>& out_keys)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile && profile->valid(), void());

    out_keys.clear();

    auto[tx, ty] = profile->getNumTiles(lod);

    for (unsigned c = 0; c < tx; ++c)
    {
        for (unsigned r = 0; r < ty; ++r)
        {
            out_keys.push_back(TileKey(lod, c, r, profile));
        }
    }
}

GeoExtent
Profile::calculateExtent( unsigned int lod, unsigned int tileX, unsigned int tileY ) const
{
    double width, height;
    getTileDimensions(lod, width, height);

    double xmin = getExtent().xMin() + (width * (double)tileX);
    double ymax = getExtent().yMax() - (height * (double)tileY);
    double xmax = xmin + width;
    double ymin = ymax - height;

    return GeoExtent( getSRS(), xmin, ymin, xmax, ymax );
}

bool
Profile::isEquivalentTo(Profile::ptr rhs) const
{
    return rhs && getFullSignature() == rhs->getFullSignature();
}

bool
Profile::isHorizEquivalentTo(Profile::ptr rhs) const
{
    return rhs && getHorizSignature() == rhs->getHorizSignature();
}

void
Profile::getTileDimensions(unsigned int lod, double& out_width, double& out_height) const
{
    out_width  = _extent.width() / (double)_numTilesWideAtLod0;
    out_height = _extent.height() / (double)_numTilesHighAtLod0;

    double factor = double(1u << lod);
    out_width /= (double)factor;
    out_height /= (double)factor;
}

std::pair<unsigned, unsigned>
Profile::getNumTiles(unsigned lod) const
{
    unsigned out_tiles_wide = _numTilesWideAtLod0;
    unsigned out_tiles_high = _numTilesHighAtLod0;

    double factor = double(1u << lod);
    out_tiles_wide *= factor;
    out_tiles_high *= factor;

    return std::make_pair(out_tiles_wide, out_tiles_high);
}

unsigned int
Profile::getLevelOfDetailForHorizResolution( double resolution, int tileSize ) const
{
    if ( tileSize <= 0 || resolution <= 0.0 ) return 23;

    double tileRes = (_extent.width() / (double)_numTilesWideAtLod0) / (double)tileSize;
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
        if (out_clamped && !getExtent().isWholeEarth())
            *out_clamped = true;
        return getExtent();
    }

    // begin by transforming the input extent to this profile's SRS.
    GeoExtent inputInMySRS = input.transform(getSRS());

    if (inputInMySRS.valid())
    {
        // Compute the intersection of the two:
        GeoExtent intersection = inputInMySRS.intersectionSameSRS(getExtent());

        // expose whether clamping took place:
        if (out_clamped != 0L)
        {
            *out_clamped = intersection != getExtent();
        }

        return intersection;
    }

    else
    {
        // The extent transformation failed, probably due to an out-of-bounds condition.
        // Go to Plan B: attempt the operation in lat/long
        auto geo_srs = getSRS()->getGeographicSRS();

        // get the input in lat/long:
        GeoExtent gcs_input =
            input.getSRS()->isGeographic() ?
            input :
            input.transform(geo_srs);

        // bail out on a bad transform:
        if (!gcs_input.valid())
            return GeoExtent::INVALID;

        // bail out if the extent's do not intersect at all:
        if (!gcs_input.intersects(_latlong_extent, false))
            return GeoExtent::INVALID;

        // clamp it to the profile's extents:
        GeoExtent clamped_gcs_input = GeoExtent(
            gcs_input.getSRS(),
            clamp(gcs_input.xMin(), _latlong_extent.xMin(), _latlong_extent.xMax()),
            clamp(gcs_input.yMin(), _latlong_extent.yMin(), _latlong_extent.yMax()),
            clamp(gcs_input.xMax(), _latlong_extent.xMin(), _latlong_extent.xMax()),
            clamp(gcs_input.yMax(), _latlong_extent.yMin(), _latlong_extent.yMax()));

        if (out_clamped)
            *out_clamped = (clamped_gcs_input != gcs_input);

        // finally, transform the clamped extent into this profile's SRS and return it.
        GeoExtent result =
            clamped_gcs_input.getSRS()->isEquivalentTo(this->getSRS().get()) ?
            clamped_gcs_input :
            clamped_gcs_input.transform(this->getSRS());

        if (!result.valid())
        {
            ROCKY_DEBUG << LC << "clamp&xform: input=" << input.toString() << ", output=" << result.toString() << std::endl;
        }

        return result;
    }    
}

unsigned
Profile::getEquivalentLOD(Profile::ptr rhsProfile, unsigned rhsLOD) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(rhsProfile!=nullptr, rhsLOD);

    //If the profiles are equivalent, just use the incoming lod
    if (this->isHorizEquivalentTo(rhsProfile))
        return rhsLOD;

    static Profile::ptr ggProfile = Profile::create(Profile::GLOBAL_GEODETIC);
    static Profile::ptr smProfile = Profile::create(Profile::SPHERICAL_MERCATOR);

    // Special check for geodetic to mercator or vise versa, they should match up in LOD.
    // TODO not sure about this.. -gw
    if ((rhsProfile->isEquivalentTo(smProfile) && isEquivalentTo(ggProfile)) ||
        (rhsProfile->isEquivalentTo(ggProfile) && isEquivalentTo(smProfile)))
    {
        return rhsLOD;
    }

    double rhsWidth, rhsHeight;
    rhsProfile->getTileDimensions(rhsLOD, rhsWidth, rhsHeight);

    // safety catch
    if (equivalent(rhsWidth, 0.0) || equivalent(rhsHeight, 0.0))
    {
        ROCKY_WARN << LC << "getEquivalentLOD: zero dimension" << std::endl;
        return rhsLOD;
    }

    auto rhsSRS = rhsProfile->getSRS();
    double rhsTargetHeight = rhsSRS->transformUnits(rhsHeight, getSRS());

    int currLOD = 0;
    int destLOD = currLOD;

    double delta = DBL_MAX;

    // Find the LOD that most closely matches the resolution of the incoming key.
    // We use the closest (under or over) so that you can match back and forth between profiles and be sure to get the same results each time.
    while (true)
    {
        double prevDelta = delta;

        double w, h;
        getTileDimensions(currLOD, w, h);

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
Profile::getLOD(double height) const
{
    int currLOD = 0;
    int destLOD = currLOD;

    double delta = DBL_MAX;

    // Find the LOD that most closely matches the target height in this profile.
    while (true)
    {
        double prevDelta = delta;

        double w, h;
        getTileDimensions(currLOD, w, h);

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
    if (!getSRS()->isHorizEquivalentTo(input.getSRS()))
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
