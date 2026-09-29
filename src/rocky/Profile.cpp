/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Profile.h"
#include "TileKey.h"
#include "Math.h"
#include "Utils.h"
#include "json.h"
#include <tuple>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define LC "[Profile] "

void
Profile::setup(const SRS& srs, const Box& bounds, unsigned width0, unsigned height0,
    const Box& geodeticBounds, const std::vector<Profile>& subprofiles)
{
    if (srs.valid())
    {
        Box b;
        unsigned tx = width0;
        unsigned ty = height0;

        if (bounds.valid())
        {
            b = bounds;
        }
        else
        {
            b = srs.bounds();
        }

        if (tx == 0 || ty == 0)
        {
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

        _shared->extent = GeoExtent(srs, b);

        _shared->numTilesBaseX = tx;
        _shared->numTilesBaseY = ty;

        if (geodeticBounds.valid())
        {
            _shared->geodeticExtent = GeoExtent(srs.geodeticSRS(), geodeticBounds);
        }
        else
        {
            // automatically calculate the lat/long extents:
            _shared->geodeticExtent = srs.isGeodetic() ?
                _shared->extent :
                _shared->extent.transform(srs.geodeticSRS());
        }

        // Extent equality is tolerant and SRS definitions can have equivalent spellings.
        // Hash only exact invariants; TileKey adds level/x/y to distribute tiles within each profile.
        _shared->hash = 0;
        hashCombine(_shared->hash, std::hash<unsigned>()(tx));
        hashCombine(_shared->hash, std::hash<unsigned>()(ty));
        hashCombine(_shared->hash, std::hash<bool>()(srs.isGeodetic()));
        hashCombine(_shared->hash, std::hash<bool>()(srs.isGeocentric()));

        _shared->subprofiles = subprofiles;
    }
}

// invalid profile
Profile::Profile()
{
    _shared = std::make_shared<Data>();
}

bool
Profile::equivalentTo(const Profile& rhs) const
{
    if (!valid() || !rhs.valid())
        return false;

    if (_shared == rhs._shared)
        return true;

    if (_shared->hash != rhs._shared->hash)
        return false;

    if (_shared->numTilesBaseY != rhs._shared->numTilesBaseY)
        return false;

    if (_shared->numTilesBaseX != rhs._shared->numTilesBaseX)
        return false;

    if (_shared->extent != rhs._shared->extent)
        return false;

    if (_shared->geodeticExtent != rhs._shared->geodeticExtent)
        return false;

    return _shared->extent.srs().horizontallyEquivalentTo(rhs._shared->extent.srs());
}

bool
Profile::operator < (const Profile& rhs) const
{
    if (hash() != rhs.hash())
        return hash() < rhs.hash();
    if (equivalentTo(rhs))
        return false;

    // Break collisions without allocating serialized profiles on tile-sorting paths.
    const auto orderKey = [](const Profile& p)
    {
        const auto& e = p.extent();
        const auto& g = p.geodeticExtent();
        const auto tiles = p.numTiles(0);
        return std::make_tuple(e.xmin(), e.ymin(), e.width(), e.height(),
            g.xmin(), g.ymin(), g.width(), g.height(), tiles.x, tiles.y, std::cref(p.srs().definition()));
    };
    return orderKey(*this) < orderKey(rhs);
}

Profile::Profile(const std::string& wellKnownName)
{
    _shared = std::make_shared<Data>();
    setup(wellKnownName);
}

Profile::Profile(const SRS& srs, const Box& bounds, unsigned x_tiles_at_lod0, unsigned y_tiles_at_lod0, 
    const Box& geodeticBounds, const std::vector<Profile>& subprofiles)
{
    _shared = std::make_shared<Data>();
    setup(srs, bounds, x_tiles_at_lod0, y_tiles_at_lod0, geodeticBounds, subprofiles);
}

void
Profile::setup(const std::string& in_name)
{
    auto name = trim(in_name);

    if (ciEquals(name, "plate-carree") ||
        ciEquals(name, "plate-carre") ||
        ciEquals(name, "eqc-wgs84"))
    {
        _shared->wellKnownName = name;

        // Yes I know this is not really Plate Carre but it will stand in for now.
        glm::dvec3 ex;
        SRS::WGS84.to(SRS::PLATE_CARREE).transform(glm::dvec3(180, 90, 0), ex);
        setup(SRS::PLATE_CARREE, Box(-ex.x, -ex.y, ex.x, ex.y), 2, 1);
    }
    else if (ciEquals(name, "global-geodetic"))
    {
        _shared->wellKnownName = "global-geodetic";
        setup(SRS::WGS84, Box(-180.0, -90.0, 180.0, 90.0), 2, 1);
    }
    else if (ciEquals(name, "spherical-mercator"))
    {
        _shared->wellKnownName = "spherical-mercator";
        setup(SRS::SPHERICAL_MERCATOR, SRS::SPHERICAL_MERCATOR.bounds(), 1, 1);
    }
    else if (name.find("+proj=longlat") == 0)
    {
        setup(SRS(name), Box(-180.0, -90.0, 180.0, 90.0), 2, 1);
    }
    else if (ciEquals(name, "qsc+z"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=90 +lon_0=0"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, -180.0, 45.0, 180.0, 90.0);
    }
    else if (ciEquals(name, "qsc-z"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=-90 +lon_0=0"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, -180.0, -90.0, 180.0, -45.0);
    }
    else if (ciEquals(name, "qsc+x"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=0"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, -45.0, -45.0, 45.0, 45.0);
    }
    else if (ciEquals(name, "qsc-x"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=180"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, 135.0, -45.0, 225.0, 45.0);
    }
    else if (ciEquals(name, "qsc+y"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=90"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, 45.0, -45.0, 125.0, 45.0);
    }
    else if (ciEquals(name, "qsc-y"))
    {
        _shared->wellKnownName = name;
        setup(
            SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=-90"),
            Box(-6378137, -6378137, 6378137, 6378137),
            2, 2);
        _shared->geodeticExtent = GeoExtent(SRS::WGS84, -135.0, -45.0, -45.0, 45.0);
    }
    else if (ciEquals(name, "global-qsc") || ciEquals(name, "qsc"))
    {
        _shared->wellKnownName = "global-qsc";

        const Box qscbox(-6378137, -6378137, 6378137, 6378137);

        setup(
            SRS::WGS84,
            Box(-180.0, -90.0, 180.0, 90.0),  // extent
            1, 1,                             // num tiles at LOD 0 (not used in QSC)
            Box(-180.0, -90.0, 180.0, 90.0),  // geodetic extent
            {
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=90 +lon_0=0"), qscbox, 2, 2, Box(-180.0, 45.0, 180.0, 90.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=-90 +lon_0=0"), qscbox, 2, 2, Box(-180.0, -90.0, 180.0, -45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=0"), qscbox, 2, 2, Box(-45.0, -45.0, 45.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=90"), qscbox, 2, 2, Box(45.0, -45.0, 125.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=180"), qscbox, 2, 2, Box(135.0, -45.0, 225.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +ellps=WGS84 +lat_0=0 +lon_0=-90"), qscbox, 2, 2, Box(-135.0, -45.0, -45.0, 45.0))
            });
    }
    else if (ciEquals(name, "moon"))
    {
        _shared->wellKnownName = "moon";

        const Box qscbox(-1737400, -1737400, 1737400, 1737400);

        setup(
            SRS::MOON, 
            Box(-180.0, -90.0, 180.0, 90.0),  // extent
            1, 1,                             // num tiles at LOD 0 (not used in QSC)
            Box(-180.0, -90.0, 180.0, 90.0),  // geodetic extent
            {
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=90 +lon_0=0"), qscbox, 2, 2, Box(-180.0, 45.0, 180.0, 90.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=-90 +lon_0=0"), qscbox, 2, 2, Box(-180.0, -90.0, 180.0, -45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=0 +lon_0=0"), qscbox, 2, 2, Box(-45.0, -45.0, 45.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=0 +lon_0=90"), qscbox, 2, 2, Box(45.0, -45.0, 125.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=0 +lon_0=180"), qscbox, 2, 2, Box(135.0, -45.0, 225.0, 45.0)),
                Profile(SRS("+wktext +proj=qsc +units=m +R=1737400 +lat_0=0 +lon_0=-90"), qscbox, 2, 2, Box(-135.0, -45.0, -45.0, 45.0))
            });
    }
    else if (!name.empty() && name[0] == '{')
    {
        // Try the old osgEarth-style profile JSON encoding.
        // Example:
        //   {"profile":{"tx":"1","ty":"2","srs":"+proj=longlat +ellps=WGS84 +no_defs","vdatum":{},"xmax":"180","xmin":"-180","ymax":"90","ymin":"-90"}}
        std::string tx, ty, srs, vdatum, xmin, ymin, xmax, ymax;

        const auto j = parse_json(name);
        auto profile_it = j.find("profile");
        if (profile_it != j.end())
        {
            auto& p = *profile_it;
            get_to(p, "tx", tx);
            get_to(p, "num_tiles_wide_at_lod_0", tx); // alias
            get_to(p, "ty", ty);
            get_to(p, "num_tiles_high_at_lod_0", ty); // alias
            get_to(p, "xmin", xmin);
            get_to(p, "ymin", ymin);
            get_to(p, "xmax", xmax);
            get_to(p, "ymax", ymax);
            get_to(p, "srs", srs);
            get_to(p, "vdatum", vdatum);

            // special handling for the egm96 vertican datum.
            // todo: figure out a better generic approach (or don't since this is just backcompat stuff)
            if (srs.length() > 5 && ciEquals(srs.substr(0, 5), "epsg:") && ciEquals(vdatum, "egm96"))
            {
                srs += "+5773";
            }

            setup(
                SRS(srs),
                Box(std::atof(xmin.c_str()), std::atof(ymin.c_str()), std::atof(xmax.c_str()), std::atof(ymax.c_str())),
                std::atoi(tx.c_str()), std::atoi(ty.c_str()),
                Box{}, {});
        }
    }
}

bool
Profile::valid() const {
    return _shared && _shared->extent.valid();
}

const SRS&
Profile::srs() const {
    return _shared->extent.srs();
}

const GeoExtent&
Profile::extent() const {
    return _shared->extent;
}

const GeoExtent&
Profile::geodeticExtent() const {
    return _shared->geodeticExtent;
}

const std::string&
Profile::wellKnownName() const {
    return _shared->wellKnownName;
}

std::string
Profile::to_json() const
{
    json temp;
    ROCKY_NAMESPACE::to_json(temp, *this);
    return temp.dump();
}

void
Profile::from_json(const std::string& input)
{
    auto j = parse_json(input);
    ROCKY_NAMESPACE::from_json(j, *this);
}

Profile
Profile::overrideSRS(const SRS& srs) const
{
    return Profile(
        srs,
        Box(_shared->extent.xmin(), _shared->extent.ymin(), _shared->extent.xmax(), _shared->extent.ymax()),
        _shared->numTilesBaseX, _shared->numTilesBaseY);
}

std::vector<TileKey>
Profile::rootKeys() const
{
    return allKeysAtLOD(0);
}

std::vector<TileKey>
Profile::allKeysAtLOD(unsigned lod) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), {});

    std::vector<TileKey> result;

    auto[tx, ty] = numTiles(lod);

    for (unsigned c = 0; c < tx; ++c)
    {
        for (unsigned r = 0; r < ty; ++r)
        {
            result.push_back(TileKey(lod, c, r, *this));
        }
    }

    return result;
}

GeoExtent
Profile::tileExtent(unsigned lod, unsigned tileX, unsigned tileY) const
{
    auto [width, height] = tileDimensions(lod);

    double xmin = extent().xmin() + (width * (double)tileX);
    double ymax = extent().ymax() - (height * (double)tileY);
    double xmax = xmin + width;
    double ymin = ymax - height;

    return GeoExtent(srs(), xmin, ymin, xmax, ymax);
}

Profile::TileDims
Profile::tileDimensions(unsigned int lod) const
{
    double out_width  = _shared->extent.width() / (double)_shared->numTilesBaseX;
    double out_height = _shared->extent.height() / (double)_shared->numTilesBaseY;

    double factor = double(1u << lod);
    out_width /= (double)factor;
    out_height /= (double)factor;

    return { out_width, out_height };
}

Profile::NumTiles
Profile::numTiles(unsigned lod) const
{
    unsigned out_tiles_wide = _shared->numTilesBaseX;
    unsigned out_tiles_high = _shared->numTilesBaseY;

    auto factor = 1u << lod;
    out_tiles_wide *= factor;
    out_tiles_high *= factor;

    return { out_tiles_wide, out_tiles_high };
}

unsigned int
Profile::levelOfDetailForHorizResolution( double resolution, int tileSize ) const
{
    if ( tileSize <= 0 || resolution <= 0.0 ) return 23;

    double tileRes = (_shared->extent.width() / (double)_shared->numTilesBaseX) / (double)tileSize;
    unsigned int level = 0;
    while( tileRes > resolution ) 
    {
        level++;
        tileRes *= 0.5;
    }
    return level;
}

unsigned
Profile::levelOfDetail(double height) const
{
    auto dims = tileDimensions(0);

    // Compute LOD: at LOD n, the height is baseHeight / (2^n)
    // Solve for n: n = log2(baseHeight / rhsTargetHeight)
    int computed_lod = static_cast<int>(std::round(std::log2(dims.y / height)));
    return (unsigned)std::max(computed_lod, 0);
}

std::string
Profile::toReadableString() const
{
    if (!wellKnownName().empty())
    {
        return wellKnownName();
    }
    else
    {
        return to_json();
    }
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

                if (obj.isComposite())
                {
                    auto sp = json::array();
                    for (auto& subprofile : obj.subprofiles())
                    {
                        json subprofile_json;
                        to_json(subprofile_json, subprofile);
                        sp.emplace_back(subprofile_json);
                    }
                    set(j, "subprofiles", sp);
                }
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

            if (j.contains("subprofiles"))
            {
                auto j_subprofiles = j.at("subprofiles");
                if (j_subprofiles.is_array())
                {
                    obj.subprofiles().clear();
                    obj.subprofiles().reserve(j_subprofiles.size());

                    for(auto& j_subprofile : j_subprofiles)
                    {
                        Profile subprofile;
                        from_json(j_subprofile, subprofile);
                        if (subprofile.valid())
                        {
                            obj.subprofiles().emplace_back(subprofile);
                        }
                    }
                }
            }

        }
        else
        {
            obj = Profile();
        }
    }
}

