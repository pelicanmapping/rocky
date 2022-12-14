/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SRS.h"
#include "Cube.h"
#include "LocalTangentPlane.h"
#include "Math.h"
#include "VerticalDatum.h"
#include "Geoid.h"

#include <ogr_spatialref.h>
#include <cpl_conv.h>

#define LC "[SRS] "

using namespace rocky;
using namespace rocky::util;

namespace
{
    std::string getOGRAttrValue(void* _handle, const std::string& name, int child_num = 0, bool lowercase = false)
    {
        const char* val = OSRGetAttrValue(_handle, name.c_str(), child_num);
        if (val)
        {
            return lowercase ? util::toLower(val) : val;
        }
        return "";
    }

    void geodeticToGeocentric(std::vector<dvec3>& points, const Ellipsoid& em)
    {
        for (unsigned i = 0; i < points.size(); ++i)
        {
            points[i] = em.geodeticToGeocentric(points[i]);
        }
    }

    void geocentricToGeodetic(std::vector<dvec3>& points, const Ellipsoid& em)
    {
        for (unsigned i = 0; i < points.size(); ++i)
        {
            points[i] = em.geocentricToGeodetic(points[i]);
        }
    }

    // Make a MatrixTransform suitable for use with a Locator object based on the given extents.
    // Calling Locator::setTransformAsExtents doesn't work with OSG 2.6 due to the fact that the
    // _inverse member isn't updated properly.  Calling Locator::setTransform works correctly.
    dmat4
    getTransformFromExtents(double minX, double minY, double maxX, double maxY)
    {
        return dmat4(
            maxX - minX, 0.0, 0.0, 0.0,
            0.0, maxY - minY, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            minX, minY, 0.0, 1.0);
    }
}

//------------------------------------------------------------------------

SRS::ThreadLocalData::ThreadLocalData() :
    _handle(nullptr),
    _workspace(nullptr),
    _workspaceSize(0u),
    _threadId(std::this_thread::get_id())
{
    //nop
}

SRS::ThreadLocalData::~ThreadLocalData()
{
    if (_workspace)
        delete[] _workspace;

    // Causing a crash under GDAL3/PROJ6 - comment out until further notice
    // This only happens on program exist anyway
#if GDAL_VERSION_MAJOR < 3
    for (auto& xformEntry : _xformCache)
    {
        optional<TransformInfo>& ti = xformEntry.second;
        if (ti.has_value() && ti->_handle != nullptr)
            OCTDestroyCoordinateTransformation(ti->_handle);
    }
#endif

    if (_handle)
    {
        OSRDestroySpatialReference(_handle);
    }
}

// Static SRS fetcher
shared_ptr<SRS>
SRS::get(const std::string& horiz, const std::string& vert)
{
    return get(Key(horiz, vert, false));
}

shared_ptr<SRS>
SRS::get(const Key& key)
{
    static std::map<Key, shared_ptr<SRS>> _srs_cache;
    static Mutex _cache_mutex;

    ScopedMutexLock lock(_cache_mutex);
    shared_ptr<SRS>& entry = _srs_cache[key];
    if (!entry)
    {
        if (key.horizLower == "unified-cube")
            entry = std::shared_ptr<contrib::CubeSRS>(new contrib::CubeSRS(key)); // contrib::CubeSRS::create(key);
        else
            entry = std::shared_ptr<SRS>(new SRS(key)); // std::make_shared<SRS>(key); // SRS::construct(key);
    }
    return entry->valid() ? entry : nullptr;
}

SRS::SRS(const Key& key) :
    _valid(true),
    _initialized(false),
    _domain(GEOGRAPHIC),
    _is_mercator(false),
    _is_north_polar(false),
    _is_south_polar(false),
    _is_cube(false),
    _is_user_defined(false),
    _is_ltp(false),
    _is_spherical_mercator(false),
    _ellipsoidId(0u),
    _local("OE.SRS.Local"),
    _mutex("OE.SRS")
{
    // shortcut for spherical-mercator:
    // https://wiki.openstreetmap.org/wiki/EPSG:3857
    if (key.horizLower == "spherical-mercator" ||
        key.horizLower == "global-mercator" ||
        key.horizLower == "web-mercator" ||
        key.horizLower == "epsg:3857" ||
        key.horizLower == "epsg:900913" ||
        key.horizLower == "epsg:102100" ||
        key.horizLower == "epsg:102113" ||
        key.horizLower == "epsg:3785" ||
        key.horizLower == "epsg:3587" ||
        key.horizLower == "osgeo:41001")
    {
        // note the use of nadgrids=@null (see http://proj.maptools.org/faq.html)
        _setup.name = "Spherical Mercator";
        _setup.type = INIT_PROJ;
#if(GDAL_VERSION_MAJOR >= 3)
        _setup.horiz = "+proj=webmerc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +towgs84=0,0,0,0,0,0,0 +wktext +no_defs";
#else
        _setup.horiz = "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +towgs84=0,0,0,0,0,0,0 +wktext +no_defs";
#endif
        _setup.vert = key.vertLower;
    }

    // true ellipsoidal ("world") mercator:
    // https://epsg.io/3395
    // https://gis.stackexchange.com/questions/259121/transformation-functions-for-epsg3395-projection-vs-epsg3857
    else if (
        key.horizLower == "world-mercator" ||
        key.horizLower == "epsg:3395" ||
        key.horizLower == "epsg:54004" ||
        key.horizLower == "epsg:9804" ||
        key.horizLower == "epsg:3832")
    {
        _setup.name = "World Mercator (WGS84)";
        _setup.type = INIT_PROJ;
        _setup.horiz = "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs";
        _setup.vert = key.vertLower;
    }

    // common WGS84:
    else if (
        key.horizLower == "epsg:4326" ||
        key.horizLower == "wgs84")
    {
        _setup.name = "WGS84";
        _setup.type = INIT_PROJ;
        _setup.horiz = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs";
        _setup.vert = key.vertLower;
    }

    // WGS84 Plate Carre:
    else if (
        key.horizLower == "plate-carre" ||
        key.horizLower == "plate-carree")
    {
        // https://proj4.org/operations/projections/eqc.html
        _setup.name = "Plate Carree";
        _setup.type = INIT_PROJ;
        _setup.horiz = "+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +units=m +ellps=WGS84 +datum=WGS84 +no_defs";
        _setup.vert = key.vertLower;
    }

    // custom srs for the unified cube
    else if (
        key.horizLower == "unified-cube")
    {
        _setup.name = "Unified Cube";
        _setup.type = INIT_USER;
        _setup.horiz = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs";
        _is_cube = true;
    }

    else if (
        key.horizLower.find('+') == 0)
    {
        //_setup.name = key.horiz;
        _setup.type = INIT_PROJ;
        _setup.horiz = key.horiz;
    }
    else if (
        key.horizLower.find("epsg:") == 0 ||
        key.horizLower.find("osgeo:") == 0)
    {
        _setup.name = key.horiz;
        _setup.type = INIT_PROJ;
        _setup.horiz = std::string("+init=") + key.horiz;
    }
    else if (
        key.horizLower.find("projcs") == 0 ||
        key.horizLower.find("geogcs") == 0)
    {
        //_setup.name = key.horiz;
        _setup.type = INIT_WKT;
        _setup.horiz = key.horiz;
    }
    else
    {
        // Try to set it from the user input.  This will handle things like CRS:84
        // createFromUserInput will actually handle all valid inputs from GDAL,
        // so we might be able to simplify this function and just call createFromUserInput.
        //_setup.name = key.horiz;
        _setup.type = INIT_USER;
        _setup.horiz = key.horiz;
    }

    // next, resolve the vertical SRS:
    if (!key.vert.empty() && !ciEquals(key.vert, "geodetic"))
    {
        _vdatum = VerticalDatum::get(key.vert);
        if (!_vdatum)
        {
            ROCKY_WARN << LC << "Failed to locate vertical datum \"" << key.vert << "\"" << std::endl;
        }
    }

    _key = key;

    // create a handle for this thread and establish validity.
    init();
}

SRS::ThreadLocalData&
SRS::getThreadLocal() const
{
    auto& local = _local.get();

    if (local._handle == nullptr)
    {
        local._threadId = std::this_thread::get_id();

        if (_setup.srcHandle != nullptr)
        {
            local._handle = OSRClone(_setup.srcHandle);
            if (!local._handle)
            {
                ROCKY_WARN << LC << "Failed to clone an existing handle" << std::endl;
                _valid = false;
            }
        }

        else
        {
            local._handle = OSRNewSpatialReference(nullptr);
            OGRErr error = OGRERR_INVALID_HANDLE;

            if (_setup.type == INIT_PROJ)
            {
                error = OSRImportFromProj4(local._handle, _setup.horiz.c_str());
            }
            else if (_setup.type == INIT_WKT)
            {
                char buf[8192];
                char* buf_ptr = &buf[0];
                if (_setup.horiz.length() < 8192)
                {
                    strcpy(buf, _setup.horiz.c_str());

                    error = OSRImportFromWkt(local._handle, &buf_ptr);

                    if (error == OGRERR_NONE)
                    {
                        //TODO
                        //fixWKT();
                    }
                }
                else
                {
                    ROCKY_WARN << LC << "BUFFER OVERFLOW - INTERNAL ERROR\n";
                    _valid = false;
                }
            }
            else
            {
                error = OSRSetFromUserInput(local._handle, _setup.horiz.c_str());
            }

            if (error == OGRERR_NONE)
            {
            }
            else
            {
                ROCKY_DEBUG << LC << "Failed to create SRS from \"" << _setup.horiz << "\"" << std::endl;
                OSRDestroySpatialReference(local._handle);
                local._handle = nullptr;
                _valid = false;
            }
        }
    }

    if (local._threadId != std::this_thread::get_id())
    {
        ROCKY_WARN << LC << "Thread Safety Violation at line " ROCKY_STRINGIFY(__LINE__) << std::endl;
    }

    return local;
}

const Box&
SRS::getBounds() const
{
    return _bounds;
}

void*
SRS::getHandle() const
{
    return getThreadLocal()._handle;
}

SRS::~SRS()
{
    //nop
}

bool
SRS::isGeographic() const
{
    return _domain == GEOGRAPHIC;
}

bool
SRS::isGeodetic() const
{
    return isGeographic() && (_vdatum == nullptr);
}

bool
SRS::isProjected() const
{
    return _domain == PROJECTED;
}

bool
SRS::isGeocentric() const
{
    return _domain == GEOCENTRIC;
}

const std::string&
SRS::getName() const
{
    return _name;
}

const Ellipsoid&
SRS::getEllipsoid() const
{
    return _ellipsoid;
}

const std::string&
SRS::getDatumName() const
{
    return _datum;
}

const Units&
SRS::getUnits() const
{
    return _units;
}

double
SRS::getReportedLinearUnits() const
{
    return _reportedLinearUnits;
}

const std::string&
SRS::getWKT() const
{
    return _wkt;
}

shared_ptr<VerticalDatum>
SRS::getVerticalDatum() const
{
    return _vdatum;
}

const SRS::Key&
SRS::getKey() const
{
    return _key;
}

const std::string&
SRS::getHorizInitString() const
{
    return _key.horiz;
}

const std::string&
SRS::getVertInitString() const
{
    return _key.vert;
}

bool
SRS::isEquivalentTo(const SRS* rhs) const
{
    return _isEquivalentTo(rhs, true);
}

bool
SRS::isHorizEquivalentTo(const SRS* rhs) const
{
    return _isEquivalentTo(rhs, false);
}

bool
SRS::isVertEquivalentTo(const SRS* rhs) const
{
    // vertical equivalence means the same vertical datum and the same
    // reference ellipsoid.
    return
        _vdatum.get() == rhs->_vdatum.get() &&
        _ellipsoidId == rhs->_ellipsoidId;
}

bool
SRS::_isEquivalentTo(
    const SRS* rhs,
    bool considerVDatum) const
{
    if (this == rhs)
        return true;

    if (!valid())
        return false;

    if (rhs == nullptr || !rhs->valid())
        return false;

    if (isGeographic() != rhs->isGeographic() ||
        isMercator() != rhs->isMercator() ||
        isGeocentric() != rhs->isGeocentric() ||
        isSphericalMercator() != rhs->isSphericalMercator() ||
        isNorthPolar() != rhs->isNorthPolar() ||
        isSouthPolar() != rhs->isSouthPolar() ||
        isUserDefined() != rhs->isUserDefined() ||
        isCube() != rhs->isCube() ||
        isLTP() != rhs->isLTP())
    {
        return false;
    }

    if (isGeocentric() && rhs->isGeocentric())
        return true;

    if (considerVDatum && (_vdatum.get() != rhs->_vdatum.get()))
        return false;

    if (_key.horizLower == rhs->_key.horizLower &&
        (!considerVDatum || (_key.vertLower == rhs->_key.vertLower)))
    {
        return true;
    }

    if (_proj4 == rhs->_proj4)
        return true;

    if (_wkt == rhs->_wkt)
        return true;

    if (this->isGeographic() && rhs->isGeographic())
    {
        return
            equivalent(getEllipsoid().getRadiusEquator(), rhs->getEllipsoid().getRadiusEquator()) &&
            equivalent(getEllipsoid().getRadiusPolar(), rhs->getEllipsoid().getRadiusPolar());
    }

    // last resort, since it requires the lock
    void* myHandle = getHandle();
    void* rhsHandle = rhs->getHandle();

    return
        myHandle &&
        rhsHandle &&
        OSRIsSame(myHandle, rhsHandle) == TRUE;
}

shared_ptr<SRS>
SRS::getGeographicSRS() const
{
    if (isGeographic())
    {
        return get(getKey());
    }

    if (_is_spherical_mercator)
    {
        return get("wgs84", _key.vertLower);
    }

    if (_geo_srs == nullptr)
    {
        util::ScopedMutexLock lock(_mutex);

        if (_geo_srs == nullptr) // double-check pattern
        {
            // temporary SRS to build the WKT
            void* temp_handle = OSRNewSpatialReference(nullptr);
            int err = OSRCopyGeogCSFrom(temp_handle, getHandle());
            if (err == OGRERR_NONE)
            {
                char* wktbuf;
                if (OSRExportToWkt(temp_handle, &wktbuf) == OGRERR_NONE)
                {
                    _geo_srs = get(std::string(wktbuf), _key.vertLower);
                    CPLFree(wktbuf);
                }
            }
            OSRDestroySpatialReference(temp_handle);
        }
    }

    return _geo_srs;
}

shared_ptr<SRS>
SRS::getGeodeticSRS() const
{
    auto geog = getGeographicSRS();
    if (geog)
        return SRS::get(Key(geog->getKey().horiz, "", false));
    else
        return nullptr;
#if 0
    if (isGeodetic())
    {
        return get(getKey());
    }

    if (_geodetic_srs == nullptr)
    {
        util::ScopedMutexLock lock(_mutex);

        if (_geodetic_srs == nullptr) // double check pattern
        {
            // temporary SRS to build the WKT
            void* temp_handle = OSRNewSpatialReference(nullptr);
            int err = OSRCopyGeogCSFrom(temp_handle, getHandle());
            if (err == OGRERR_NONE)
            {
                char* wktbuf;
                if (OSRExportToWkt(temp_handle, &wktbuf) == OGRERR_NONE)
                {
                    _geodetic_srs = get(std::string(wktbuf), "");
                    CPLFree(wktbuf);
                }
            }
            OSRDestroySpatialReference(temp_handle);
        }
    }

    return _geodetic_srs;
#endif
}

shared_ptr<SRS>
SRS::getGeocentricSRS() const
{
    auto geog = getGeographicSRS();
    if (geog)
        return SRS::get(Key(geog->getKey().horiz, geog->getKey().vert, true));
    else
        return nullptr;
#if 0
    if (isGeocentric())
    {
        return get(getKey());
    }

    if (_geocentric_srs == nullptr)
    {
        util::ScopedMutexLock lock(_mutex);

        if (_geocentric_srs == nullptr) // double-check pattern
        {
            // temporary SRS to build the WKT
            void* temp_handle = OSRNewSpatialReference(nullptr);
            int err = OSRCopyGeogCSFrom(temp_handle, getHandle());
            if (err == OGRERR_NONE)
            {
                char* wktbuf;
                if (OSRExportToWkt(temp_handle, &wktbuf) == OGRERR_NONE)
                {
                    _geocentric_srs = get(Key(std::string(wktbuf), ""));
                    _geocentric_srs->_domain = GEOCENTRIC;
                    CPLFree(wktbuf);
                }
            }
            OSRDestroySpatialReference(temp_handle);
        }
    }

    return _geocentric_srs;
#endif
}

shared_ptr<SRS>
SRS::createTangentPlaneSRS(const dvec3& origin) const
{
    if (!valid())
        return nullptr;

    dvec3 lla;
    auto srs = getGeographicSRS();
    if (srs && transform(origin, srs.get(), lla))
    {
        const Key& key = srs->getKey();
        return std::make_shared<TangentPlaneSRS>(key, lla);
    }
    else
    {
        ROCKY_WARN << LC << "Unable to create LTP SRS" << std::endl;
        return nullptr;
    }
}

shared_ptr<SRS>
SRS::createTransMercFromLongitude(const Angle& lon) const
{
    if (!valid())
        return nullptr;

    // note. using tmerc with +lat_0 <> 0 is sloooooow.
    std::string datum = getDatumName();
    std::string horiz = Stringify()
        << "+proj=tmerc +lat_0=0"
        << " +lon_0=" << lon.as(Units::DEGREES)
        << " +datum=" << (!datum.empty() ? "WGS84" : datum);

    return SRS::get(horiz, getVertInitString());
}

shared_ptr<SRS>
SRS::createUTMFromLonLat(const Angle& lon, const Angle& lat) const
{
    if (!valid())
        return nullptr;

    // note. UTM is up to 10% faster than TMERC for the same meridian.
    unsigned zone = 1 + (unsigned)floor((lon.as(Units::DEGREES) + 180.0) / 6.0);
    std::string datum = getDatumName();
    std::string horiz = Stringify()
        << "+proj=utm +zone=" << zone
        << (lat.as(Units::DEGREES) < 0 ? " +south" : "")
        << " +datum=" << (!datum.empty() ? "WGS84" : datum);

    return SRS::get(horiz, getVertInitString());
}

shared_ptr<SRS>
SRS::createEquirectangularSRS() const
{
    if (!valid())
        return nullptr;

    return SRS::get(
        "+proj=eqc +units=m +no_defs",
        getVertInitString());
}

bool
SRS::isMercator() const
{
    return _is_mercator;
}

bool
SRS::isSphericalMercator() const
{
    return _is_spherical_mercator;
}

bool
SRS::isNorthPolar() const
{
    return _is_north_polar;
}

bool
SRS::isSouthPolar() const
{
    return _is_south_polar;
}

bool
SRS::isUserDefined() const
{
    return _is_user_defined;
}

bool
SRS::isCube() const
{
    return _is_cube;
}

bool
SRS::createLocalToWorld(
    const dvec3& xyz, 
    dmat4& out_local2world) const
{
    if (!valid())
        return false;

    if (isProjected() && !isCube())
    {
        dvec3 world;
        if (!transformToWorld(xyz, world))
            return false;
       
        out_local2world = glm::translate(dmat4(1), world);
    }
    else if (isGeocentric())
    {
        out_local2world = _ellipsoid.geocentricToLocalToWorld(xyz);
    }
    else
    {
        // convert to ECEF:
        dvec3 ecef;
        if (!transform(xyz, getGeocentricSRS().get(), ecef))
            return false;

        // and create the matrix.
        out_local2world = _ellipsoid.geocentricToLocalToWorld(ecef);
    }
    return true;
}

bool
SRS::createWorldToLocal(
    const dvec3& xyz,
    dmat4& out_world2local) const
{
    dmat4 local2world(1.0);
    if (!createLocalToWorld(xyz, local2world))
        return false;
    out_world2local = glm::inverse(local2world);
    //out_world2local.invert(local2world);
    return true;
}


bool
SRS::transform(
    const dvec3& input,
    const SRS* outputSRS,
    dvec3& output) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outputSRS != nullptr, false);

    if (!valid())
        return false;

    std::vector<dvec3> v(1, input);

    if (transformInPlace(v, outputSRS))
    {
        output = v[0];
        return true;
    }
    return false;
}


bool
SRS::transformInPlace(
    std::vector<dvec3>& points,
    const SRS* outputSRS) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outputSRS != nullptr, false);

    if (!valid())
        return false;

    // trivial equivalency:
    if (isEquivalentTo(outputSRS))
        return true;

    bool success = false;

    // do the pre-transformation pass:
    auto inputSRS = this;

    if (!preTransform(points, &inputSRS))
        return false;

    if (inputSRS->isGeocentric() && !outputSRS->isGeocentric())
    {
        auto outputGeoSRS = outputSRS->getGeodeticSRS();
        geocentricToGeodetic(points, outputGeoSRS->getEllipsoid());
        return outputGeoSRS->transformInPlace(points, outputSRS);
    }

    else if (!inputSRS->isGeocentric() && outputSRS->isGeocentric())
    {
        auto outputGeoSRS = outputSRS->getGeodeticSRS();
        success = inputSRS->transformInPlace(points, outputGeoSRS.get());
        geodeticToGeocentric(points, outputGeoSRS->getEllipsoid());
        return success;
    }

    // if the points are starting as geographic, do the Z's first to avoid an unneccesary
    // transformation in the case of differing vdatums.
    bool z_done = false;
    if (inputSRS->isGeographic())
    {
        z_done = inputSRS->transformZ(points, outputSRS, true);
    }

    auto& local = getThreadLocal();

    // move the xy data into straight arrays that OGR can use
    unsigned count = points.size();

    if (count * 2 > local._workspaceSize)
    {
        if (local._workspace)
            delete[] local._workspace;
        local._workspace = new double[count * 2];
        local._workspaceSize = count * 2;
    }

    double* x = local._workspace;
    double* y = local._workspace + count;

    for (unsigned i = 0; i < count; i++)
    {
        x[i] = points[i].x;
        y[i] = points[i].y;
    }

    success = inputSRS->transformXYPointArrays(local, x, y, count, outputSRS);

    if (success)
    {
        if (inputSRS->isProjected() && outputSRS->isGeographic())
        {
            // special case: when going from projected to geographic, clamp the 
            // points to the maximum geographic extent. Sometimes the conversion from
            // a global/projected SRS (like mercator) will result in *slightly* invalid
            // geographic points (like long=180.000003), so this addresses that issue.
            for (unsigned i = 0; i < count; i++)
            {
                points[i].x = clamp(x[i], -180.0, 180.0);
                points[i].y = clamp(y[i], -90.0, 90.0);
            }
        }
        else
        {
            for (unsigned i = 0; i < count; i++)
            {
                points[i].x = x[i];
                points[i].y = y[i];
            }
        }
    }

    if (success)
    {
        // calculate the Zs if we haven't already done so
        if (!z_done)
        {
            z_done = inputSRS->transformZ(
                points,
                outputSRS,
                outputSRS->isGeographic());
        }

        // run the user post-transform code
        if (!outputSRS->postTransform(points, nullptr))
            return false;
    }

    return success;
}


bool
SRS::transform2D(
    double x, double y,
    const SRS* outputSRS,
    double& out_x, double& out_y) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outputSRS != nullptr, false);

    if (!valid())
        return false;

    dvec3 temp(x, y, 0);
    bool ok = transform(temp, outputSRS, temp);
    if (ok) {
        out_x = temp.x;
        out_y = temp.y;
    }
    return ok;
}


bool
SRS::transformXYPointArrays(
    ThreadLocalData& local,
    double*  x,
    double*  y,
    unsigned count,
    const SRS* out_srs) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(out_srs != nullptr, false);

    if (!valid())
        return false;

    // Transform the X and Y values inside an exclusive GDAL/OGR lock
    optional<TransformInfo>& xform = local._xformCache[out_srs->getWKT()];
    if (!xform.has_value())
    {
        xform->_handle = OCTNewCoordinateTransformation(local._handle, out_srs->getHandle());

        if (xform->_handle == nullptr)
        {
            ROCKY_WARN << LC
                << "SRS xform not possible:" << std::endl
                << "    From => " << getName() << std::endl
                << "    To   => " << out_srs->getName() << std::endl;

            ROCKY_WARN << LC << "INPUT: " << getWKT() << std::endl
                << "OUTPUT: " << out_srs->getWKT() << std::endl;

            const char* errmsg = CPLGetLastErrorMsg();
            ROCKY_WARN << LC << "ERROR: " << (errmsg ? errmsg : "do not know") << std::endl;

            xform->_handle = nullptr;
            xform->_failed = true;

            return false;
        }
    }

    if (xform->_failed)
    {
        return false;
    }

    return OCTTransform(xform->_handle, count, x, y, 0L) > 0;
}


bool
SRS::transformZ(
    std::vector<dvec3>& points,
    const SRS* outputSRS,
    bool pointsAreLatLong) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outputSRS != nullptr, false);

    if (!valid())
        return false;

    VerticalDatum::ptr outVDatum = outputSRS->getVerticalDatum();

    // same vdatum, no xformation necessary.
    if (_vdatum == outVDatum)
        return true;

    Units inUnits = _vdatum ? _vdatum->getGeoid()->units : Units::METERS;
    Units outUnits = outVDatum ? outVDatum->getGeoid()->units : inUnits;

    if (isGeographic() || pointsAreLatLong)
    {
        for (unsigned i = 0; i < points.size(); ++i)
        {
            if (_vdatum)
            {
                // to HAE:
                points[i].z = _vdatum->msl2hae(points[i].y, points[i].x, points[i].z);
            }

            // do the units conversion:
            points[i].z = inUnits.convertTo(outUnits, points[i].z);

            if (outVDatum)
            {
                // to MSL:
                points[i].z = outVDatum->hae2msl(points[i].y, points[i].x, points[i].z);
            }
        }
    }

    else // need to xform input points
    {
        // copy the points and convert them to geographic coordinates (lat/long with the same Z):
        std::vector<dvec3> geopoints(points);
        transformInPlace(geopoints, getGeographicSRS().get());

        for (unsigned i = 0; i < geopoints.size(); ++i)
        {
            if (_vdatum)
            {
                // to HAE:
                points[i].z = _vdatum->msl2hae(geopoints[i].y, geopoints[i].x, points[i].z);
            }

            // do the units conversion:
            points[i].z = inUnits.convertTo(outUnits, points[i].z);

            if (outVDatum)
            {
                // to MSL:
                points[i].z = outVDatum->hae2msl(geopoints[i].y, geopoints[i].x, points[i].z);
            }
        }
    }

    return true;
}

bool
SRS::transformToWorld(
    const dvec3& input,
    dvec3& output) const
{
    if (!valid())
        return false;

    if (isGeographic() || isCube())
    {
        return transform(input, getGeocentricSRS().get(), output);
    }
    else // isProjected
    {
        output = input;
        if (_vdatum)
        {
            dvec3 geo(input);
            if (!transform(input, getGeographicSRS().get(), geo))
                return false;

            output.z = _vdatum->msl2hae(geo.y, geo.x, input.z);
        }
        return true;
    }
}

bool
SRS::transformFromWorld(
    const dvec3& world,
    dvec3& output,
    double* out_haeZ) const
{
    if (isGeographic() || isCube())
    {
        bool ok = getGeocentricSRS()->transform(world, this, output);
        if (ok && out_haeZ)
        {
            if (_vdatum)
                *out_haeZ = _vdatum->msl2hae(output.y, output.x, output.z);
            else
                *out_haeZ = output.z;
        }
        return ok;
    }
    else // isProjected
    {
        output = world;

        if (out_haeZ)
            *out_haeZ = world.z;

        if (_vdatum)
        {
            // get the geographic coords by converting x/y/hae -> lat/long/msl:
            dvec3 lla;
            if (!transform(world, getGeographicSRS().get(), lla))
                return false;

            output.z = lla.z;
        }

        return true;
    }
}

double
SRS::transformUnits(
    double input,
    shared_ptr<SRS> outSRS,
    const Angle& latitude) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outSRS != nullptr, input);

    if (this->isProjected() && outSRS->isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS->getUnits(),
            outSRS->getEllipsoid().metersToLongitudinalDegrees(
                getUnits().convertTo(Units::METERS, input),
                latitude.as(Units::DEGREES)));
    }
    else if (this->isGeocentric() && outSRS->isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS->getUnits(),
            outSRS->getEllipsoid().metersToLongitudinalDegrees(input, latitude));
    }
    else if (this->isGeographic() && outSRS->isProjected())
    {
        return Units::METERS.convertTo(
            outSRS->getUnits(),
            outSRS->getEllipsoid().longitudinalDegreesToMeters(
                getUnits().convertTo(Units::DEGREES, input),
                latitude.as(Units::DEGREES)));
    }
    else if (this->isGeographic() && outSRS->isGeocentric())
    {
        return outSRS->getEllipsoid().longitudinalDegreesToMeters(
            getUnits().convertTo(Units::DEGREES, input),
            latitude.as(Units::DEGREES));
    }
    else // both projected or both geographic.
    {
        return getUnits().convertTo(outSRS->getUnits(), input);
    }
}

double
SRS::transformUnits(
    const Distance& distance,
    shared_ptr<SRS> outSRS,
    const Angle& latitude)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outSRS != nullptr, distance.getValue());

    if (distance.getUnits().isLinear() && outSRS->isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS->getUnits(),
            outSRS->getEllipsoid().metersToLongitudinalDegrees(
                distance.as(Units::METERS),
                latitude.as(Units::DEGREES)));
    }
    else if (distance.getUnits().isAngular() && outSRS->isProjected())
    {
        return Units::METERS.convertTo(
            outSRS->getUnits(),
            outSRS->getEllipsoid().longitudinalDegreesToMeters(
                distance.as(Units::DEGREES),
                latitude.as(Units::DEGREES)));
    }
    else // both projected or both geographic.
    {
        return distance.as(outSRS->getUnits());
    }
}

bool
SRS::transformExtentToMBR(
    shared_ptr<SRS> to_srs,
    double& in_out_xmin,
    double& in_out_ymin,
    double& in_out_xmax,
    double& in_out_ymax) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(to_srs != nullptr, false);

    if (!valid())
        return false;

    // Transform all points and take the maximum bounding rectangle the resulting points
    std::vector<dvec3> v;

    // Start by clamping to the out_srs' legal bounds, if possible.
    // TODO: rethink this to be more generic.
    if (isGeographic() && (to_srs->isMercator() || to_srs->isSphericalMercator()))
    {
        Profile::ptr merc = Profile::create(Profile::SPHERICAL_MERCATOR);
        in_out_ymin = clamp(in_out_ymin, merc->getLatLongExtent().yMin(), merc->getLatLongExtent().yMax());
        in_out_ymax = clamp(in_out_ymax, merc->getLatLongExtent().yMin(), merc->getLatLongExtent().yMax());
    }

    double height = in_out_ymax - in_out_ymin;
    double width = in_out_xmax - in_out_xmin;

    // first point is a centroid. This we will use to make sure none of the corner points
    // wraps around if the target SRS is geographic.
    v.push_back(dvec3(in_out_xmin + width * 0.5, in_out_ymin + height * 0.5, 0)); // centroid.

    // add the four corners
    v.push_back(dvec3(in_out_xmin, in_out_ymin, 0)); // ll
    v.push_back(dvec3(in_out_xmin, in_out_ymax, 0)); // ul
    v.push_back(dvec3(in_out_xmax, in_out_ymax, 0)); // ur
    v.push_back(dvec3(in_out_xmax, in_out_ymin, 0)); // lr

    //We also sample along the edges of the bounding box and include them in the 
    //MBR computation in case you are dealing with a projection that will cause the edges
    //of the bounding box to be expanded.  This was first noticed when dealing with converting
    //Hotline Oblique Mercator to WGS84

    //Sample the edges
    unsigned int numSamples = 5;
    double dWidth = width / (numSamples - 1);
    double dHeight = height / (numSamples - 1);

    //Left edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v.push_back(dvec3(in_out_xmin, in_out_ymin + dHeight * (double)i, 0));
    }

    //Right edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v.push_back(dvec3(in_out_xmax, in_out_ymin + dHeight * (double)i, 0));
    }

    //Top edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v.push_back(dvec3(in_out_xmin + dWidth * (double)i, in_out_ymax, 0));
    }

    //Bottom edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v.push_back(dvec3(in_out_xmin + dWidth * (double)i, in_out_ymin, 0));
    }

    if (transformInPlace(v, to_srs.get()))
    {
        in_out_xmin = DBL_MAX;
        in_out_ymin = DBL_MAX;
        in_out_xmax = -DBL_MAX;
        in_out_ymax = -DBL_MAX;

        // For a geographic target, make sure the new extents contain the centroid
        // because they might have wrapped around or run into a precision failure.
        // v[0]=centroid, v[1]=LL, v[2]=UL, v[3]=UR, v[4]=LR
        if (to_srs->isGeographic())
        {
            if (v[1].x > v[0].x || v[2].x > v[0].x) in_out_xmin = -180.0;
            if (v[3].x < v[0].x || v[4].x < v[0].x) in_out_xmax = 180.0;
        }

        // enforce an MBR:
        for (unsigned int i = 0; i < v.size(); i++)
        {
            in_out_xmin = std::min(v[i].x, in_out_xmin);
            in_out_ymin = std::min(v[i].y, in_out_ymin);
            in_out_xmax = std::max(v[i].x, in_out_xmax);
            in_out_ymax = std::max(v[i].y, in_out_ymax);
        }

        return true;
    }

    return false;
}

bool
SRS::transformGrid(
    shared_ptr<SRS> to_srs,
    double in_xmin, double in_ymin,
    double in_xmax, double in_ymax,
    double* x, double* y,
    unsigned int numx, unsigned int numy) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(to_srs != nullptr, false);

    if (!valid())
        return false;

    std::vector<dvec3> points;

    const double dx = (in_xmax - in_xmin) / (numx - 1);
    const double dy = (in_ymax - in_ymin) / (numy - 1);

    unsigned int pixel = 0;
    double fc = 0.0;
    for (unsigned int c = 0; c < numx; ++c, ++fc)
    {
        const double dest_x = in_xmin + fc * dx;
        double fr = 0.0;
        for (unsigned int r = 0; r < numy; ++r, ++fr)
        {
            const double dest_y = in_ymin + fr * dy;

            points.push_back(dvec3(dest_x, dest_y, 0));
            pixel++;
        }
    }

    if (transformInPlace(points, to_srs.get()))
    {
        for (unsigned i = 0; i < points.size(); ++i)
        {
            x[i] = points[i].x;
            y[i] = points[i].y;
        }
        return true;
    }
    return false;
}

void
SRS::init()
{
    void* handle = getHandle();

    if (!handle)
    {
        _valid = false;
        return;
    }

    if (_is_ltp)
    {
        _is_user_defined = true;
        _domain = PROJECTED;
    }
    else if (_is_cube)
    {
        _domain = PROJECTED;
    }
    else if (_key.geocentric)
    {
        _domain = GEOCENTRIC;
    }
    else //if (!isGeocentric())
    {
        _domain = OSRIsGeographic(handle) != 0 ?
            GEOGRAPHIC : PROJECTED;
    }

    // Give the SRS a name if it doesn't have one:
    if (!_setup.name.empty())
    {
        _name = _setup.name;
    }

    // extract the ellipsoid parameters:
    int err;

    _ellipsoid.setSemiMajorAxis(OSRGetSemiMajor(handle, &err));
    _ellipsoid.setSemiMinorAxis(OSRGetSemiMinor(handle, &err));

    // unique ID for comparing ellipsoids quickly:
    _ellipsoidId = hashString(Stringify()
        << std::fixed << std::setprecision(10)
        << _ellipsoid.getSemiMajorAxis() << ";" << _ellipsoid.getSemiMinorAxis());

    // try to get an ellipsoid name:
    _ellipsoid.setName(getOGRAttrValue(handle, "SPHEROID"));

    // extract the projection:
    if (_name.empty() || _name == "unnamed" || _name == "unknown")
    {
        if (isGeographic())
        {
            _name = getOGRAttrValue(handle, "GEOGCS", 0);
            if (_name.empty()) _name = getOGRAttrValue(handle, "GEOGCRS");
        }
        else
        {
            _name = getOGRAttrValue(handle, "PROJCS");
            if (_name.empty()) _name = getOGRAttrValue(handle, "PROJCRS");
        }
    }
    std::string projection = getOGRAttrValue(handle, "PROJECTION");
    std::string projection_lc = util::toLower(projection);

    // check for the Mercator projection:
    _is_mercator = !projection_lc.empty() && projection_lc.find("mercator") == 0;

    // check for spherical mercator (a special case)
    _is_spherical_mercator = _is_mercator && equivalent(
        _ellipsoid.getSemiMajorAxis(),
        _ellipsoid.getSemiMinorAxis());

    // check for the Polar projection:
    if (!projection_lc.empty() && projection_lc.find("polar_stereographic") != std::string::npos)
    {
        double lat = util::as<double>(getOGRAttrValue(handle, "latitude_of_origin", 0, true), -90.0);
        _is_north_polar = lat > 0.0;
        _is_south_polar = lat < 0.0;
    }
    else
    {
        _is_north_polar = false;
        _is_south_polar = false;
    }

    // Try to extract the horizontal datum
    _datum = getOGRAttrValue(handle, "DATUM", 0, true);

    // Extract the base units:
    std::string units = getOGRAttrValue(handle, "UNIT", 0, true);
    double unitMultiplier = util::as<double>(getOGRAttrValue(handle, "UNIT", 1, true), 1.0);
    if (isGeographic())
        _units = Units(units, units, Units::TYPE_ANGULAR, unitMultiplier);
    else
        _units = Units(units, units, Units::TYPE_LINEAR, unitMultiplier);

    _reportedLinearUnits = OSRGetLinearUnits(handle, nullptr);

    // Try to extract the PROJ4 initialization string:
    char* proj4buf;
    if (OSRExportToProj4(handle, &proj4buf) == OGRERR_NONE)
    {
        _proj4 = proj4buf;
        CPLFree(proj4buf);
    }

    // Try to extract the OGC well-known-text (WKT) string:
    char* wktbuf;
    if (OSRExportToWkt(handle, &wktbuf) == OGRERR_NONE)
    {
        _wkt = wktbuf;
        CPLFree(wktbuf);
    }

    if (_name == "unnamed" || _name == "unknown" || _name.empty())
    {
        StringTable proj4_tok;
        StringTokenizer(_proj4, proj4_tok);
        if (proj4_tok["+proj"] == "utm")
        {
            _name = Stringify() << "UTM " << proj4_tok["+zone"];
        }
        else
        {
            _name =
                isGeographic() && !_datum.empty() ? _datum :
                isGeographic() && !_ellipsoid.getName().empty() ? _ellipsoid.getName() :
                isGeographic() ? "Geographic" :
                isGeocentric() ? "Geocentric" :
                isCube() ? "Unified Cube" :
                isLTP() ? "Tangent Plane" :
                !projection.empty() ? projection :
                _is_spherical_mercator ? "Spherical Mercator" :
                _is_mercator ? "Mercator" :
                (!_proj4.empty() ? _proj4 :
                    "Projected");
        }
    }

    // Build a 'normalized' initialization key.
    if (!_proj4.empty())
    {
        _key.horiz = _proj4;
        _key.horizLower = toLower(_key.horiz);
    }
    else if (!_wkt.empty())
    {
        _key.horiz = _wkt;
        _key.horizLower = toLower(_key.horiz);
    }
    if (_vdatum)
    {
        _key.vert = _vdatum->getName();
        _key.vertLower = toLower(_key.vert);
    }

    // Guess the appropriate bounds for this SRS.
    int isNorth;
    _bounds = Box(-FLT_MAX, -FLT_MAX, 0.0, FLT_MAX, FLT_MAX, 0.0);

#if 0 // this always returns false as of GDAL 3.1.3, so omit until later
#if GDAL_VERSION_MAJOR >= 3
    double wlong, elong, slat, nlat;
    if (OSRGetAreaOfUse(handle, &wlong, &slat, &elong, &nlat, nullptr) &&
        wlong > -1000.0)
    {
        osg::ref_ptr<const SRS> geo = getGeographicSRS();
        geo->transform2D(wlong, slat, this, _bounds.xMin(), _bounds.yMin());
        geo->transform2D(elong, nlat, this, _bounds.xMax(), _bounds.yMax());
        ROCKY_INFO << LC << "Bounds: " << _bounds.toString() << std::endl;
    }
#endif
#endif

    if (_bounds.xmin == -FLT_MAX)
    {
        if (isGeographic() || isGeocentric())
        {
            _bounds = Box(-180.0, -90.0, 0.0, 180.0, 90.0, 0.0);
        }
        else if (isMercator() || isSphericalMercator())
        {
            _bounds = Box(MERC_MINX, MERC_MINY, 0.0, MERC_MAXX, MERC_MAXY, 0.0);
        }
        else if (OSRGetUTMZone(handle, &isNorth))
        {
            if (isNorth)
                _bounds = Box(166000, 0, 0.0, 834000, 9330000, 0.0);
            else
                _bounds = Box(166000, 1116915, 0.0, 834000, 10000000, 0.0);
        }
    }
}



SRS::Key::Key()
{
}

SRS::Key::Key(const std::string& h, const std::string& v, bool geoc) :
    horiz(h),
    horizLower(util::toLower(h)),
    vert(v),
    vertLower(toLower(v)),
    geocentric(geoc)
{
    hash = std::hash<std::string>()(horizLower + vertLower + (geoc?"ecef":""));
}

bool
SRS::Key::operator == (const Key& rhs) const
{
    return
        horizLower == rhs.horizLower &&
        vertLower == rhs.vertLower &&
        geocentric == rhs.geocentric;
}

bool
SRS::Key::operator < (const Key& rhs) const
{
    int h = horizLower.compare(rhs.horizLower);
    if (h < 0) return true;
    if (h > 0) return false;
    int v = vertLower.compare(rhs.vertLower);
    if (v < 0) return true;
    if (v > 0) return false;
    if (geocentric == false && rhs.geocentric == true) return true;
    return false;
}
