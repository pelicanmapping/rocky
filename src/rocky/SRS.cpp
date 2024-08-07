/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SRS.h"
#include "Math.h"
#include "Threading.h"
#include "Instance.h"

#include <filesystem>
#include <proj.h>

#define LC "[SRS] "

using namespace ROCKY_NAMESPACE;

ROCKY_ABOUT(proj, std::to_string(PROJ_VERSION_MAJOR) + "." + std::to_string(PROJ_VERSION_MINOR));

namespace
{
    void redirect_proj_log(void* user, int level, const char* msg)
    {
        if (msg)
        {
            Log()->info("PROJ says: " + std::string(msg));
        }
    }

    inline bool starts_with(const std::string& s, const char* pattern) {
        return s.rfind(pattern, 0) == 0;
    }

    inline bool contains(const std::string& s, const char* pattern) {
        return s.find(pattern) != std::string::npos;
    }


    //! Per thread proj threading context.
    thread_local PJ_CONTEXT* g_pj_thread_local_context = nullptr;

    const Ellipsoid default_ellipsoid = { };
    const Box empty_box = { };
    const std::string empty_string = { };

    //! Entry in the per-thread SRS data cache
    struct SRSEntry
    {
        PJ* pj = nullptr;
        PJ_TYPE crs_type = PJ_TYPE_UNKNOWN;
        PJ_TYPE horiz_crs_type = PJ_TYPE_UNKNOWN;
        PJ_TYPE vert_crs_type = PJ_TYPE_UNKNOWN;
        optional<Box> bounds;
        std::string wkt;
        std::string proj;
        Ellipsoid ellipsoid = { };
        std::string error;
    };

    //! SRS data factory and PROJ main interface
    struct SRSFactory : public std::unordered_map<std::string, SRSEntry>
    {
        //! destroy cache entries and threading context upon descope
        ~SRSFactory()
        {
            for (auto iter = begin(); iter != end(); ++iter)
            {
                if (iter->second.pj)
                {
                    proj_destroy(iter->second.pj);
                }
            }

            if (g_pj_thread_local_context)
                proj_context_destroy(g_pj_thread_local_context);
        }

        PJ_CONTEXT* threading_context()
        {
            if (g_pj_thread_local_context == nullptr)
            {
                g_pj_thread_local_context = proj_context_create();
                proj_log_func(g_pj_thread_local_context, nullptr, redirect_proj_log);
            }
            return g_pj_thread_local_context;
        }

        const std::string& get_error_message(const std::string& def)
        {
            auto iter = find(def);
            if (iter == end())
                return empty_string; // shouldn't happen
            else
                return iter->second.error;
        }

        //! retrieve or create a PJ projection object based on the provided definition string,
        //! which may be a proj string, a WKT string, an espg identifer, or a well-known alias
        //! like "spherical-mercator" or "wgs84".
        SRSEntry& get_or_create(const std::string& def)
        {
            ROCKY_PROFILE_FUNCTION();

            auto ctx = threading_context();

            auto iter = find(def);
            if (iter == end())
            {
                PJ* pj = nullptr;

                std::string to_try = def;
                std::string ndef = util::toLower(def);

                //TODO: Think about epsg:4979, which is supposedly a 3D version of epsg::4326.

                // process some common aliases
                if (ndef == "wgs84" || ndef == "global-geodetic")
                    to_try = "epsg:4979";
                else if (ndef == "spherical-mercator")
                    to_try = "epsg:3785";
                else if (ndef == "geocentric" || ndef == "ecef")
                    to_try = "epsg:4978";
                else if (ndef == "plate-carree" || ndef == "plate-carre")
                    to_try = "epsg:32663";

                // try to determine whether this ia WKT so we can use the right create function
                auto wkt_dialect = proj_context_guess_wkt_dialect(ctx, to_try.c_str());
                if (wkt_dialect != PJ_GUESSED_NOT_WKT)
                {
                    pj = proj_create_from_wkt(ctx, to_try.c_str(), nullptr, nullptr, nullptr);
                }
                else
                {
                    // if it's a PROJ string, be sure to add the +type=crs
                    if (contains(ndef, "+proj"))
                    {
                        if (!contains(ndef, "proj=pipeline") && !contains(ndef, "type=crs"))
                        {
                            to_try += " +type=crs";
                        }
                    }
                    else
                    {
                        // perhaps it's an EPSG string, in which case we must lower-case it so it
                        // works on case-sensitive file systems
                        // https://github.com/pyproj4/pyproj/blob/9283f962e4792da2a7f05ba3735c1ed7f3479502/pyproj/crs/crs.py#L111
                        util::replace_in_place(to_try, "+init=EPSG", "+init=epsg");
                    }

                    pj = proj_create(ctx, to_try.c_str());
                }

                // store in the cache (even if it failed)
                SRSEntry& new_entry = (*this)[def];
                new_entry.pj = pj;

                if (pj == nullptr)
                {
                    // store any error in the cache entry
                    auto err_no = proj_context_errno(ctx);
                    new_entry.error = proj_errno_string(err_no);
                }
                else
                {
                    // extract the type
                    new_entry.crs_type = proj_get_type(pj);

                    if (new_entry.crs_type == PJ_TYPE_COMPOUND_CRS)
                    {
                        constexpr int HORIZONTAL = 0, VERTICAL = 1;

                        PJ* horiz = proj_crs_get_sub_crs(ctx, pj, HORIZONTAL);
                        if (horiz)
                            new_entry.horiz_crs_type = proj_get_type(horiz);

                        PJ* vert = proj_crs_get_sub_crs(ctx, pj, VERTICAL);
                        if (vert)
                            new_entry.vert_crs_type = proj_get_type(vert);
                    }
                    else
                    {
                        new_entry.horiz_crs_type = new_entry.crs_type;
                    }

                    // extract the ellipsoid parameters.
                    // if this fails, the entry will contain the default WGS84 ellipsoid, and that is ok.
                    PJ* ellps = proj_get_ellipsoid(ctx, pj);
                    if (ellps)
                    {
                        double semi_major, semi_minor, inv_flattening;
                        int is_semi_minor_computed;
                        proj_ellipsoid_get_parameters(ctx, ellps, &semi_major, &semi_minor, &is_semi_minor_computed, &inv_flattening);
                        proj_destroy(ellps);
                        new_entry.ellipsoid = Ellipsoid(semi_major, semi_minor);
                    }

                    // extract the WKT
                    const char* wkt = proj_as_wkt(ctx, pj, PJ_WKT2_2019, nullptr);
                    if (wkt)
                        new_entry.wkt = wkt;

                    // extract the PROJ string
                    const char* proj = proj_as_proj_string(ctx, pj, PJ_PROJ_5, nullptr);
                    if (proj)
                        new_entry.proj = proj;
                }

                return new_entry;
            }
            else
            {
                return iter->second;
            }
        }

        //! fetch the projection type
        PJ_TYPE get_horiz_crs_type(const std::string& def)
        {
            ROCKY_PROFILE_FUNCTION();
            return get_or_create(def).horiz_crs_type;
        }

        //! fetch the ellipsoid associated with an SRS definition
        //! that was previously created
        const Ellipsoid& get_ellipsoid(const std::string& def)
        {
            ROCKY_PROFILE_FUNCTION();
            return get_or_create(def).ellipsoid;
        };

        //! Get the computed bounds of a projection (or guess at them)
        const Box& get_bounds(const std::string& def)
        {
            ROCKY_PROFILE_FUNCTION();

            auto ctx = threading_context();

            SRSEntry& entry = get_or_create(def);

            if (entry.pj == nullptr)
                return empty_box;

            if (entry.bounds.has_value())
                return entry.bounds.value();

            // Well known stuff:
            if (entry.horiz_crs_type == PJ_TYPE_GEOGRAPHIC_2D_CRS || entry.horiz_crs_type == PJ_TYPE_GEOGRAPHIC_3D_CRS)
            {
                entry.bounds = Box(-180, -90, 180, 90);
            }

            else if (entry.horiz_crs_type != PJ_TYPE_GEOCENTRIC_CRS)
            {
                if (contains(entry.proj, "proj=utm"))
                {
                    if (contains(entry.proj, "+south"))
                        entry.bounds = Box(166000, 1116915, 834000, 10000000);
                    else
                        entry.bounds = Box(166000, 0, 834000, 9330000);
                }

                else if (contains(entry.proj, "proj=merc"))
                {
                    // values found empirically 
                    //entry.bounds = Box(-20037508.342789244, -20048966.104014594, 20037508.342789244, 20048966.104014594);
                    entry.bounds = Box(-20037508.34278925, -20037508.34278925, 20037508.34278925, 20037508.34278925);
                }

                else if (contains(entry.proj, "proj=qsc"))
                {
                    // maximum possible values, I think
                    entry.bounds = Box(
                        -entry.ellipsoid.semiMajorAxis(),
                        -entry.ellipsoid.semiMinorAxis(),
                        entry.ellipsoid.semiMajorAxis(),
                        entry.ellipsoid.semiMinorAxis());
                }
            }

            // no good? try querying it instead:
            if (!entry.bounds.has_value())
            {
                double west_lon, south_lat, east_lon, north_lat;
                if (proj_get_area_of_use(ctx, entry.pj, &west_lon, &south_lat, &east_lon, &north_lat, nullptr) &&
                    west_lon > -1000)
                {
                    // always returns lat/long, so transform back to this srs
                    auto xform = get_or_create_operation("wgs84", def); // don't call proj_destroy on this
                    PJ_COORD LL = proj_trans(xform, PJ_FWD, PJ_COORD{ west_lon, south_lat, 0.0, 0.0 });
                    PJ_COORD UR = proj_trans(xform, PJ_FWD, PJ_COORD{ east_lon, north_lat, 0.0, 0.0 });
                    entry.bounds = Box(LL.xyz.x, LL.xyz.y, UR.xyz.x, UR.xyz.y);
                }
            }

            if (!entry.bounds.has_value())
            {
                entry.bounds = empty_box; // :(
            }

            return entry.bounds.value();
        }

        const std::string& get_wkt(const std::string& def)
        {
            ROCKY_PROFILE_FUNCTION();

            auto iter = find(def);
            if (iter == end())
            {
                get_or_create(def);
                iter = find(def);
                if (iter == end())
                    return empty_string;
            }
            return iter->second.wkt;
        }

        //! retrieve or create a transformation object
        PJ* get_or_create_operation(const std::string& firstDef, const std::string& secondDef)
        {
            ROCKY_PROFILE_FUNCTION();

            auto ctx = threading_context();

            PJ* pj = nullptr;
            std::string proj;
            std::string error;

            // make a unique identifer for the transformation
            std::string def = firstDef + "->" + secondDef;

            auto iter = find(def);

            if (iter == end())
            {
                PJ* p1 = get_or_create(firstDef).pj;
                PJ* p2 = get_or_create(secondDef).pj;
                if (p1 && p2)
                {
                    bool p1_is_crs = proj_is_crs(p1);
                    bool p2_is_crs = proj_is_crs(p2);
                    bool normalize_to_gis_coords = true;

                    PJ_TYPE p1_type = proj_get_type(p1);
                    PJ_TYPE p2_type = proj_get_type(p2);

                    if (p1_is_crs && p2_is_crs)
                    {
                        // Check for an illegal operation (PROJ 9.1.0). We do this because
                        // proj_create_crs_to_crs_from_pj() will succeed even though the operation will not
                        // process the Z input.
                        if (p1_type == PJ_TYPE_GEOGRAPHIC_2D_CRS && p2_type == PJ_TYPE_COMPOUND_CRS)
                        {
                            std::string warning = "Warning, \"" + def + "\" transforms from GEOGRAPHIC_2D_CRS to COMPOUND_CRS. Z values will be discarded. Use a GEOGRAPHIC_3D_CRS instead";
                            redirect_proj_log(nullptr, 0, warning.c_str());
                        }

                        // if they are both CRS's, just make a transform operation.
                        pj = proj_create_crs_to_crs_from_pj(ctx, p1, p2, nullptr, nullptr);

                        if (pj && proj_get_type(pj) != PJ_TYPE_UNKNOWN)
                        {
                            const char* pcstr = proj_as_proj_string(ctx, pj, PJ_PROJ_5, nullptr);
                            if (pcstr) proj = pcstr;
                        }
                    }

                    else if (p1_is_crs && !p2_is_crs)
                    {
                        PJ_TYPE type = proj_get_type(p1);

                        if (type == PJ_TYPE_GEOGRAPHIC_2D_CRS || type == PJ_TYPE_GEOGRAPHIC_3D_CRS)
                        {
                            proj =
                                "+proj=pipeline"
                                " +step +proj=unitconvert +xy_in=deg +xy_out=rad"
                                " +step " + std::string(proj_as_proj_string(ctx, p2, PJ_PROJ_5, nullptr));

                            pj = proj_create(ctx, proj.c_str());

                            normalize_to_gis_coords = false;
                        }
                        else
                        {
                            proj =
                                "+proj=pipeline"
                                " +step +proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs +towgs84=0,0,0"
                                " +step +proj=unitconvert +xy_in=deg +xy_out=rad"
                                " +step " + std::string(proj_as_proj_string(ctx, p2, PJ_PROJ_5, nullptr)) +
                                " +step +proj=unitconvert +xy_in=rad +xy_out=deg";

                            pj = proj_create(ctx, proj.c_str());

                            normalize_to_gis_coords = false;
                        }
                    }

                    else if (!p1_is_crs && p2_is_crs)
                    {
                        proj =
                            "+proj=pipeline"
                            " +step +inv " + std::string(proj_as_proj_string(ctx, p1, PJ_PROJ_5, nullptr)) +
                            " +step " + std::string(proj_as_proj_string(ctx, p2, PJ_PROJ_5, nullptr)) +
                            " +step +proj=unitconvert +xy_in=rad +xy_out=deg";

                        pj = proj_create(ctx, proj.c_str());

                        normalize_to_gis_coords = false;
                    }

                    else
                    {
                        proj =
                            "+proj=pipeline"
                            " +step +inv " + std::string(proj_as_proj_string(ctx, p1, PJ_PROJ_5, nullptr)) +
                            " +step " + std::string(proj_as_proj_string(ctx, p2, PJ_PROJ_5, nullptr));

                        pj = proj_create(ctx, proj.c_str());

                        normalize_to_gis_coords = false;
                    }

                    // integrate forward and backward vdatum conversions if necessary.
                    if (pj && normalize_to_gis_coords)
                    {
                        // re-order the coordinates to GIS standard long=x, lat=y
                        pj = proj_normalize_for_visualization(ctx, pj);
                    }
                }

                if (!pj && error.empty())
                {
                    auto err_no = proj_context_errno(ctx);
                    if (err_no != 0)
                    {
                        error = proj_errno_string(err_no);
                        //Instance::log().warn << error << " (\"" << def << "\")" << std::endl;
                    }
                }

                auto& new_entry = (*this)[def];
                new_entry.pj = pj;
                new_entry.proj = proj;
                new_entry.error = error;
            }
            else
            {
                pj = iter->second.pj;
            }
            return pj;
        }
    };

    // create an SRS repo per thread since proj is not thread safe.
    thread_local SRSFactory g_srs_factory;
}


#if 0
// Note! Moved these to Instance.cpp since there's a static startup dependency!
const SRS SRS::WGS84("epsg:4979");
const SRS SRS::ECEF("geocentric");
const SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
const SRS SRS::PLATE_CARREE("plate-carree");
const SRS SRS::EMPTY;
#endif

std::string
SRS::projVersion()
{
    return std::to_string(PROJ_VERSION_MAJOR) + "." + std::to_string(PROJ_VERSION_MINOR);
}

SRS::SRS()
{
    //nop
}

SRS::SRS(const std::string& h) :
    _definition(h)
{
    PJ* pj = g_srs_factory.get_or_create(h).pj;
    _valid = (pj != nullptr);

    if (_valid)
    {
        // cache things that get called a LOT
        auto type = g_srs_factory.get_horiz_crs_type(h);
        
        _isGeodetic =
            type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
            type == PJ_TYPE_GEOGRAPHIC_3D_CRS;

        _isGeocentric = type == PJ_TYPE_GEOCENTRIC_CRS;
    }
}

SRS::~SRS()
{
    //nop
}

const char*
SRS::name() const
{
    PJ* pj = g_srs_factory.get_or_create(_definition).pj;
    if (!pj) return "";
    return proj_get_name(pj);
}

bool
SRS::isGeodetic() const
{
    return valid() && _isGeodetic;
}

bool
SRS::isGeocentric() const
{
    return valid() && _isGeocentric;
}

bool
SRS::isProjected() const
{
    if (!valid())
        return false;

    auto type = g_srs_factory.get_horiz_crs_type(definition());
    return
        type == PJ_TYPE_PROJECTED_CRS;
}

bool
SRS::hasVerticalDatumShift() const
{
    if (!valid())
        return false;

    return g_srs_factory.get_or_create(definition()).vert_crs_type != PJ_TYPE_UNKNOWN;
}

bool
SRS::equivalentTo(const SRS& rhs) const
{
    if (definition().empty() || rhs.definition().empty())
        return false;

    PJ* pj1 = g_srs_factory.get_or_create(definition()).pj;
    if (!pj1)
        return false;

    PJ* pj2 = g_srs_factory.get_or_create(rhs.definition()).pj;
    if (!pj2)
        return false;

    PJ_COMPARISON_CRITERION criterion =
        _isGeodetic ? PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS :
        PJ_COMP_EQUIVALENT;

    return proj_is_equivalent_to_with_ctx(
        g_srs_factory.threading_context(), pj1, pj2, criterion);
}

bool
SRS::horizontallyEquivalentTo(const SRS& rhs) const
{
    if (definition().empty() || rhs.definition().empty())
        return false;

    if (_isGeodetic && rhs._isGeodetic && ellipsoid() == rhs.ellipsoid())
        return true;

    auto& lhs_entry = g_srs_factory.get_or_create(definition());
    PJ* pj1 = lhs_entry.pj;
    if (!pj1)
        return false;

    auto& rhs_entry = g_srs_factory.get_or_create(rhs.definition());
    PJ* pj2 = rhs_entry.pj;
    if (!pj2)
        return false;

    if (lhs_entry.crs_type != rhs_entry.crs_type)
        return false;

    // compare only the horizontal CRS components:
    PJ* lhs_pj = nullptr;
    PJ* rhs_pj = nullptr;

    lhs_pj = lhs_entry.crs_type == PJ_TYPE_COMPOUND_CRS ?
        proj_crs_get_sub_crs(g_srs_factory.threading_context(), pj1, 0) : pj1;

    rhs_pj = rhs_entry.crs_type == PJ_TYPE_COMPOUND_CRS ?
        proj_crs_get_sub_crs(g_srs_factory.threading_context(), pj2, 0) : pj2;

    PJ_COMPARISON_CRITERION criterion =
        _isGeodetic ? PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS :
        PJ_COMP_EQUIVALENT;

    return proj_is_equivalent_to_with_ctx(
        g_srs_factory.threading_context(), lhs_pj, rhs_pj, criterion);
}

const std::string&
SRS::wkt() const
{
    return g_srs_factory.get_wkt(definition());
}

const Units&
SRS::units() const
{
    return isGeodetic() ? Units::DEGREES : Units::METERS;
}

const Ellipsoid&
SRS::ellipsoid() const
{
    return g_srs_factory.get_ellipsoid(definition());
}

const Box&
SRS::bounds() const
{
    return g_srs_factory.get_bounds(definition());
}

SRSOperation
SRS::to(const SRS& rhs) const
{
    SRSOperation result;
    if (valid() && rhs.valid())
    {
        result._from = *this;
        result._to = rhs;
        result._nop = (result._from == result._to);
        return result;
    }
    return result;
}

SRS
SRS::geoSRS() const
{
    ROCKY_PROFILE_FUNCTION();

    if (isGeodetic())
        return *this;

    auto& def = g_srs_factory.get_or_create(_definition);

    PJ* pj = def.pj;
    if (pj)
    {
        if (isGeocentric())
        {
            // A bit hacky but it works.
            // We do this because proj_crs_get_geodetic_crs() on a geocentric CRS
            // just returns the same geocentric CRS. Is that a bug in proj?
            std::string proj = def.proj;
            util::replace_in_place(proj, "+proj=geocent", "+proj=longlat");
            return SRS(proj);
        }
        auto ctx = g_srs_factory.threading_context();

        PJ* pjgeo = proj_crs_get_geodetic_crs(ctx, pj);
        if (pjgeo)
        {
            std::string wkt = proj_as_wkt(ctx, pjgeo, PJ_WKT2_2019, nullptr);
            proj_destroy(pjgeo);
            return SRS(wkt);
        }
    }
    return SRS(); // invalid
}

SRS
SRS::geocentricSRS() const
{
    ROCKY_PROFILE_FUNCTION();

    auto& def = g_srs_factory.get_or_create(_definition);
    if (def.pj)
    {
        auto ctx = g_srs_factory.threading_context();
        PJ* pjgeo = proj_crs_get_geodetic_crs(ctx, def.pj);
        if (pjgeo)
        {
            std::string proj = proj_as_proj_string(ctx, pjgeo, PJ_PROJ_5, nullptr);
            util::replace_in_place(proj, "+proj=longlat", "+proj=geocent");
            proj_destroy(pjgeo);
            return SRS(proj);
        }
    }
    return SRS();
}

glm::dmat4
SRS::localToWorldMatrix(const glm::dvec3& origin) const
{
    if (!valid())
        return { };

    if (isGeodetic())
    {
        glm::dvec3 ecef;
        to(SRS::ECEF).transform(origin, ecef);
        return ellipsoid().geocentricToLocalToWorld(ecef);
    }
    else if (isGeocentric())
    {
        return ellipsoid().geocentricToLocalToWorld(origin);
    }
    else // projected
    {
        return glm::translate(glm::dmat4(1.0), origin);
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

    if (inSRS.isProjected() && outSRS.isGeodetic())
    {
        return Units::DEGREES.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().metersToLongitudinalDegrees(
                inSRS.units().convertTo(Units::METERS, input),
                latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeocentric() && outSRS.isGeodetic())
    {
        return Units::DEGREES.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().metersToLongitudinalDegrees(input, latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeodetic() && outSRS.isProjected())
    {
        return Units::METERS.convertTo(
            outSRS.units(),
            outSRS.ellipsoid().longitudinalDegreesToMeters(
                inSRS.units().convertTo(Units::DEGREES, input),
                latitude.as(Units::DEGREES)));
    }
    else if (inSRS.isGeodetic() && outSRS.isGeocentric())
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

    if (distance.units().isLinear() && outSRS.isGeodetic())
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

const std::string&
SRS::errorMessage() const
{
    return g_srs_factory.get_error_message(definition());
}

std::string
SRS::string() const
{
    if (valid())
        return g_srs_factory.get_or_create(definition()).proj;
    else
        return "";
}

SRSOperation::SRSOperation()
{
    // nop
}

SRSOperation&
SRSOperation::operator=(SRSOperation&& rhs)
{
    _from = rhs._from;
    _to = rhs._to;
    rhs._from = { };
    rhs._to = { };
    return *this;
}

SRSOperation::~SRSOperation()
{
    //nop
}

bool
SRSOperation::valid() const
{
    return _from.valid() && _to.valid() && get_handle() != nullptr;
}

void*
SRSOperation::get_handle() const
{
    return (void*)g_srs_factory.get_or_create_operation(
        _from.definition(),
        _to.definition());
}

bool
SRSOperation::forward(void* handle, double& x, double& y, double& z) const
{
    if (handle)
    {
        proj_errno_reset((PJ*)handle);
        PJ_COORD out = proj_trans((PJ*)handle, PJ_FWD, PJ_COORD{ x, y, z });
        int err = proj_errno((PJ*)handle);
        if (err != 0)
        {
            _lastError = proj_errno_string(err);
            return false;
        }
        x = out.xyz.x, y = out.xyz.y, z = out.xyz.z;
        return true;
    }
    else
        return false;
}

bool
SRSOperation::forward(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const
{
    if (handle)
    {
        proj_errno_reset((PJ*)handle);

        proj_trans_generic((PJ*)handle, PJ_FWD,
            x, stride, count,
            y, stride, count,
            z, stride, count,
            nullptr, stride, count);

        int err = proj_errno((PJ*)handle);
        if (err != 0)
        {
            _lastError = proj_errno_string(err);
            return false;
        }
        return true;
    }
    else
        return false;
}


bool
SRSOperation::inverse(void* handle, double& x, double& y, double& z) const
{
    if (handle)
    {
        proj_errno_reset((PJ*)handle);
        PJ_COORD out = proj_trans((PJ*)handle, PJ_INV, PJ_COORD{ x, y, z });
        int err = proj_errno((PJ*)handle);
        if (err != 0)
        {
            _lastError = proj_errno_string(err);
            return false;
        }
        x = out.xyz.x, y = out.xyz.y, z = out.xyz.z;
        return true;
    }
    else
        return false;
}

bool
SRSOperation::inverse(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const
{
    if (handle)
    {
        proj_errno_reset((PJ*)handle);

        proj_trans_generic((PJ*)handle, PJ_INV,
            x, stride, count,
            y, stride, count,
            z, stride, count,
            nullptr, stride, count);

        int err = proj_errno((PJ*)handle);
        if (err != 0)
        {
            _lastError = proj_errno_string(err);
            return false;
        }
        return true;
    }
    else
        return false;
}

std::string
SRSOperation::string() const
{
    return std::string(
        proj_as_proj_string(g_pj_thread_local_context, (PJ*)get_handle(), PJ_PROJ_5, nullptr));
}


#include "json.h"
namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const SRS& obj) {
        j = obj.definition();
    }
    void from_json(const json& j, SRS& obj) {
        obj = SRS(get_string(j));
    }
}
