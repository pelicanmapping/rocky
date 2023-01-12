/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SRS.h"
#include "Math.h"
#include "Notify.h"
#include "Threading.h"

#include <filesystem>
#include <proj.h>

#define LC "[SRS] "

using namespace rocky;
using namespace rocky::util;

namespace
{
    thread_local PJ_CONTEXT* g_pjctx = nullptr;

    inline bool starts_with(const std::string& s, const char* pattern) {
        return s.rfind(pattern, 0) == 0;
    }

    struct SRSRepo : public std::unordered_map<std::string, PJ*>
    {
        ~SRSRepo()
        {
            std::cout << "SRSRepo: destructor in thread " << util::getCurrentThreadId()
                << " destroying " << size() << " objects" << std::endl;

            for (auto iter = begin(); iter != end(); ++iter)
            {
                if (iter->second)
                {
                    proj_destroy(iter->second);
                }
            }

            if (g_pjctx)
                proj_context_destroy(g_pjctx);
        }

        PJ* get_or_create(const std::string& def)
        {
            PJ* result = nullptr;
            if (g_pjctx == nullptr)
            {
                g_pjctx = proj_context_create();
            }
            auto iter = find(def);
            if (iter == end())
            {
                std::string to_try = def;
                std::string ndef = util::toLower(def);

                //TODO: Think about epsg:4979, which is supposedly a 3D version of epsg::4326.

                if (ndef == "wgs84" || ndef == "global-geodetic")
                    to_try = "epsg:4326";
                else if (ndef == "spherical-mercator")
                    to_try = "epsg:3785";
                else if (ndef == "geocentric" || ndef == "ecef")
                    to_try = "epsg:4978";
                else if (ndef == "plate-carree")
                    to_try = "+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +units=m +ellps=WGS84 +datum=WGS84 +no_defs";

                auto wkt_dialect = proj_context_guess_wkt_dialect(g_pjctx, to_try.c_str());
                if (wkt_dialect != PJ_GUESSED_NOT_WKT)
                {
                    result = proj_create_from_wkt(g_pjctx, to_try.c_str(), nullptr, nullptr, nullptr);
                }
                else
                {
                    result = proj_create(g_pjctx, to_try.c_str());
                }

                if (result == nullptr)
                {
                    auto err_no = proj_context_errno(g_pjctx);
                    std::string err = proj_errno_string(err_no);
                    std::cerr << err << " (get_or_create " << def << ")" << std::endl;
                }

                (*this)[def] = result;
            }
            else
            {
                result = iter->second;
            }            
            return result;
        }

        std::string make_grid_name(const std::string& in)
        {
            if (in == "egm96")
                return "us_nga_egm96_15.tif";
            else if (!std::filesystem::path(in).has_extension())
                return in + ".tif";
            else
                return in;
        }

        PJ* get_or_create_xform(const SRS& first, const SRS& second)
        {
            PJ* result = nullptr;

            if (g_pjctx == nullptr)
            {
                g_pjctx = proj_context_create();
            }
            std::string def = first.definition() + first.vertical() + ">" + second.definition() + second.vertical();
            auto iter = find(def);
            if (iter == end())
            {
                PJ* p1 = get_or_create(first.definition());
                PJ* p2 = get_or_create(second.definition());
                if (p1 && p2)
                {
                    result = proj_create_crs_to_crs_from_pj(g_pjctx, p1, p2, nullptr, nullptr);
                    if (result)
                    {
                        bool xform_is_noop = false;

                        // integrate vdatums if necessary.
                        if (!first.vertical().empty() || !second.vertical().empty())
                        {
                            // extract the transformation string:
                            std::string str = proj_as_proj_string(g_pjctx, result, PJ_PROJ_5, nullptr);
                            
                            // if it doesn't start with pipeline, it's a noop and we need to insert a pipeline
                            if (!starts_with(str, "+proj=pipeline"))
                            {
                                str = "+proj=pipeline +step " + str;
                                xform_is_noop = true;
                            }

                            // n.b., vgridshift requires radians
                            if (!first.vertical().empty())
                            {
                                str +=
                                    " +step +proj=unitconvert +xy_in=deg +xy_out=rad"
                                    " +step +inv +proj=vgridshift +grids=" + make_grid_name(first.vertical()) +
                                    " +step +proj=unitconvert +xy_in=rad +xy_out=deg";
                            }

                            if (!second.vertical().empty())
                            {
                                str +=
                                    " +step +proj=unitconvert +xy_in=deg +xy_out=rad"
                                    " +step +proj=vgridshift +grids=" + make_grid_name(second.vertical()) +
                                    " +step +proj=unitconvert +xy_in=rad +xy_out=deg";
                            }
                            
                            // ditch the old one
                            proj_destroy(result);

                            // make the new one
                            result = proj_create(g_pjctx, str.c_str());
                        }

                        if (result && !xform_is_noop)
                        {
                            // re-order the coordinates to long=x, lat=y
                            result = proj_normalize_for_visualization(g_pjctx, result);
                        }
                    }

                    if (!result)
                    {
                        auto err_no = proj_context_errno(g_pjctx);
                        if (err_no != 0)
                        {
                            std::string err = proj_errno_string(err_no);
                            std::cerr << err << " (get_or_create_xform " << def << ")" << std::endl;
                        }
                    }
                }

                (*this)[def] = result;
            }
            else
            {
                result = iter->second;
            }
            return result;
        }
    };

    thread_local SRSRepo g_srs_lut;
}

// common ones.
SRS SRS::WGS84("epsg:4326");
SRS SRS::ECEF("geocentric");
SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
SRS SRS::PLATE_CARREE("plate-carree");
SRS SRS::EMPTY;

SRS::SRS() :
    _valid(false)
{
    //nop
}

SRS::SRS(const std::string& h, const std::string& v) :
    _definition(h),
    _vertical(v),
    _valid(false)
{
    PJ* pj = g_srs_lut.get_or_create(h);
    _valid = (pj != nullptr);
}

SRS&
SRS::operator=(SRS&& rhs)
{
    _definition = rhs._definition;
    _valid = rhs._valid;
    rhs._definition.clear();
    rhs._valid = false;
    return *this;
}

SRS::~SRS()
{
    //nop
}

const char*
SRS::name() const
{
    PJ* pj = g_srs_lut.get_or_create(_definition);
    if (!pj) return "";
    return proj_get_name(pj);
}

bool
SRS::isGeographic() const
{
    PJ* pj = g_srs_lut.get_or_create(_definition);
    if (!pj) return false;
    auto type = proj_get_type(pj);
    return
        type == PJ_TYPE_GEOGRAPHIC_CRS ||
        type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        type == PJ_TYPE_GEOGRAPHIC_3D_CRS;
}

bool
SRS::isProjected() const
{
    PJ* pj = g_srs_lut.get_or_create(_definition);
    if (!pj) return false;
    auto type = proj_get_type(pj);
    return
        type == PJ_TYPE_PROJECTED_CRS;
}

bool
SRS::isGeocentric() const
{
    PJ* pj = g_srs_lut.get_or_create(_definition);
    if (!pj) return false;
    auto type = proj_get_type(pj);
    return
        type == PJ_TYPE_GEOCENTRIC_CRS;
}

bool
SRS::isEquivalentTo(const SRS& rhs) const
{
    if (vertical() != rhs.vertical())
        return false;
    else
        return isHorizEquivalentTo(rhs);
}

bool
SRS::isHorizEquivalentTo(const SRS& rhs) const
{
    PJ* pj1 = g_srs_lut.get_or_create(definition());
    PJ* pj2 = g_srs_lut.get_or_create(rhs.definition());
    if (!pj1 || !pj2)
        return false;
    else
        return proj_is_equivalent_to_with_ctx(g_pjctx, pj1, pj2, PJ_COMP_EQUIVALENT);
}

std::string
SRS::wkt() const
{
    PJ* pj = g_srs_lut.get_or_create(definition());
    if (!pj)
        return {};
    else
        return proj_as_wkt(g_pjctx, pj, PJ_WKT2_2019, nullptr);
}

Units
SRS::units() const
{
    return isGeographic() ? Units::DEGREES : Units::METERS;
}

Ellipsoid
SRS::ellipsoid() const
{
    PJ* pj = g_srs_lut.get_or_create(definition());
    if (!pj) return {};

    PJ* ellps = proj_get_ellipsoid(g_pjctx, pj);
    if (!ellps) return {};

    double semi_major, semi_minor, inv_flattening;
    int is_semi_minor_computed;
    proj_ellipsoid_get_parameters(g_pjctx, ellps, &semi_major, &semi_minor, &is_semi_minor_computed, &inv_flattening);
    proj_destroy(ellps);
    
    return Ellipsoid(semi_major, semi_minor);
}

Box
SRS::bounds() const
{
    if (isGeocentric())
        return { };

    PJ* pj = g_srs_lut.get_or_create(definition());
    if (!pj) return {};

    double west_lon, south_lat, east_lon, north_lat;
    if (proj_get_area_of_use(g_pjctx, pj, &west_lon, &south_lat, &east_lon, &north_lat, nullptr) &&
        west_lon > -1000)
    {
        // always returns lat/long, so transform back to this srs
        dvec3 LL, UR;
        auto xform = SRS::WGS84.to(*this);
        xform(dvec3(west_lon, south_lat, 0), LL);
        xform(dvec3(east_lon, north_lat, 0), UR);
        return Box(LL.x, LL.y, UR.x, UR.y);
    }
    else
    {
        // We will have to make an educated guess.
        if (isGeographic())
        {
            return Box(-180, -90, 180, 90);
        }
        else if (isProjected())
        {
            std::string ndef = util::toLower(definition());

            if (ndef.find("proj=utm") != std::string::npos ||
                ndef.find("transverse_mercator") != std::string::npos)
            {
                if (ndef.find("+south") != std::string::npos)
                {
                    return Box(166000, 1116915, 834000, 10000000);
                }
                else
                {
                    return Box(166000, 0, 834000, 9330000);
                }
            }

            if (ndef.find("mercator") != std::string::npos)
            {
                // values taken by running transform
                return Box(-20037508.342789244, -20048966.104014594, 20037508.342789244, 20048966.104014594);
            }
        }
    }

    return { };
}

SRSTransform
SRS::to(const SRS& rhs) const
{
    SRSTransform result;
    if (valid() && rhs.valid())
    {
        result._first = *this;
        result._second = rhs;
        return result;
    }
    return result;
}

SRS
SRS::geoSRS() const
{
    PJ* pj = g_srs_lut.get_or_create(_definition);
    if (pj)
    {
        PJ* pjgeo = proj_crs_get_geodetic_crs(g_pjctx, pj);
        if (pjgeo)
        {
            std::string wkt = proj_as_wkt(g_pjctx, pjgeo, PJ_WKT2_2019, nullptr);
            proj_destroy(pjgeo);
            return SRS(wkt, vertical());
        }
    }
    return SRS(); // invalid
}

dmat4
SRS::localToWorldMatrix(const dvec3& origin) const
{
    if (!valid())
        return { };

    if (isProjected())
    {
        return glm::translate(dmat4(1.0), origin);
    }
    else if (isGeocentric())
    {
        return ellipsoid().geocentricToLocalToWorld(origin);
    }
    else // if (isGeographic())
    {
        dvec3 ecef;
        to(SRS::ECEF).transform(origin, ecef);
        return ellipsoid().geocentricToLocalToWorld(ecef);
    }
}

double
SRS::transformUnits(
    double input,
    const SRS& inSRS,
    const SRS& outSRS,
    const Angle& latitude)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(inSRS.valid() && outSRS.valid(), 0.0);

    if (inSRS.isProjected() && outSRS.isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().metersToLongitudinalDegrees(
                inSRS.units().convertTo(Units::METERS, input),
                latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeocentric() && outSRS.isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().metersToLongitudinalDegrees(input, latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeographic() && outSRS.isProjected())
    {
        return Units::METERS.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().longitudinalDegreesToMeters(
                inSRS.units().convertTo(Units::DEGREES, input),
                latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeographic() && outSRS.isGeocentric())
    {
        return outSRS.ellipsoid().longitudinalDegreesToMeters(
            inSRS.units().convertTo(Units::DEGREES, input),
            latitude.as(Units::DEGREES));
    }
    else // both projected or both geographic.
    {
        return inSRS.units().convertTo(outSRS.units(), input);
    }
}


double
SRS::transformUnits(
    const Distance& distance,
    const SRS& outSRS,
    const Angle& latitude)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(outSRS.valid(), distance.value());

    if (distance.units().isLinear() && outSRS.isGeographic())
    {
        return Units::DEGREES.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().metersToLongitudinalDegrees(
                distance.as(Units::METERS),
                latitude.as(Units::DEGREES)));
    }
    else if (distance.units().isAngular() && outSRS.isProjected())
    {
        return Units::METERS.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().longitudinalDegreesToMeters(
                distance.as(Units::DEGREES),
                latitude.as(Units::DEGREES)));
    }
    else // both projected or both geographic.
    {
        return distance.as(outSRS.units());
    }
}


SRSTransform::SRSTransform()
{
    // nop
}

SRSTransform&
SRSTransform::operator=(SRSTransform&& rhs)
{
    _first = rhs._first;
    _second = rhs._second;
    rhs._first = {};
    rhs._second = {};
    return *this;
}

SRSTransform::~SRSTransform()
{
    //nop
}

bool
SRSTransform::valid() const
{
    return
        _first.valid() &&
        _second.valid() &&
        get_handle() != nullptr;
}

void*
SRSTransform::get_handle() const
{
    return (void*)g_srs_lut.get_or_create_xform(_first, _second);
}

bool
SRSTransform::forward(void* handle, double& x, double& y, double& z) const
{
    if (handle)
    {
        //std::cout << proj_as_proj_string(g_pjctx, (PJ*)handle, PJ_PROJ_5, nullptr) << std::endl;
        proj_errno_reset((PJ*)handle);
        auto out = proj_trans((PJ*)handle, PJ_FWD, PJ_COORD{ x, y, z });
        if (proj_errno((PJ*)handle) != 0)
            return false;
        x = out.xyz.x, y = out.xyz.y, z = out.xyz.z;
        return true;
    }
    else
        return false;
}

bool
SRSTransform::inverse(void* handle, double& x, double& y, double& z) const
{
    if (handle)
    {
        proj_errno_reset((PJ*)handle);
        auto out = proj_trans((PJ*)handle, PJ_INV, PJ_COORD{ x, y, z });
        if (proj_errno((PJ*)handle) != 0)
        {
            std::string err = proj_errno_string(proj_errno((PJ*)handle));
            std::cerr << err << " (inverse)" << std::endl;
            return false;
        }
        x = out.xyz.x, y = out.xyz.y, z = out.xyz.z;
        return true;
    }
    else
        return false;
}
