/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "SRS.h"
#include "Math.h"
#include "Threading.h"
#include "Context.h"

#include <filesystem>
#include <proj.h>
#include <array>

#define LC "[SRS] "

using namespace ROCKY_NAMESPACE;

ROCKY_ABOUT(proj, std::to_string(PROJ_VERSION_MAJOR) + "." + std::to_string(PROJ_VERSION_MINOR));


// STATIC INITIALIZATION: built-in common SRS definitions
const SRS SRS::WGS84("wgs84");
const SRS SRS::ECEF("geocentric");
const SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
const SRS SRS::PLATE_CARREE("plate-carree");
const SRS SRS::MOON("moon");
const SRS SRS::EMPTY;

// STATIC INITIALIZATION: PROJ library debug message callback
std::function<void(int level, const char* msg)> SRS::projMessageCallback = nullptr;


namespace
{
    void redirect_proj_log(void* user, int level, const char* msg)
    {
        if (msg)
        {
            if (SRS::projMessageCallback)
                SRS::projMessageCallback(level, msg);
            else
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
        PJ* pj_geodetic = nullptr;
        PJ_TYPE crs_type = PJ_TYPE_UNKNOWN;
        PJ_TYPE horiz_crs_type = PJ_TYPE_UNKNOWN;
        PJ_TYPE vert_crs_type = PJ_TYPE_UNKNOWN;
        option<Box> bounds;
        option<Box> geodeticBounds;
        std::string wkt;
        std::string proj;
        Ellipsoid ellipsoid = { };
        std::string error;
        SRS geodeticSRS;
        SRS geocentricSRS;
        bool isQSC = false;
    };

    //! SRS data factory and PROJ main interface
    struct SRSFactory : public std::unordered_map<std::string, SRSEntry>
    {
        SRSFactory() = default;

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
            auto ctx = threading_context();

            auto iter = find(def);
            if (iter == end())
            {
                PJ* pj = nullptr;

                std::string to_try = util::trim(def);
                std::string ndef = util::toLower(to_try);

                const char* wkt_tags[13] = {
                    "geoccs[", "geoccrs[",
                    "geogcs[", "geogcrs[",
                    "projcs[", "projcrs[",
                    "vertcs[", "vertcrs[", "vert_cs[",
                    "compdcs[", "compd_cs[", "compoundcrs[",
                    "timecrs["
                };
                for (auto& wkt_tag : wkt_tags)
                {
                    if (util::startsWith(ndef, wkt_tag))
                    {
                        auto wkt_dialect = proj_context_guess_wkt_dialect(ctx, to_try.c_str());

                        if (wkt_dialect != PJ_GUESSED_NOT_WKT)
                        {
                            pj = proj_create_from_wkt(ctx, to_try.c_str(), nullptr, nullptr, nullptr);
                        }
                    }
                }

                if (pj == nullptr)
                {
                    //TODO: Think about epsg:4979, which is supposedly a 3D version of epsg::4326.

                    // process some common aliases
                    if (ndef == "wgs84" || ndef == "global-geodetic")
                        to_try = "epsg:4979";
                    else if (ndef == "spherical-mercator")
                        to_try = "epsg:3857"; //3785;
                    else if (ndef == "geocentric" || ndef == "ecef")
                        to_try = "epsg:4978";
                    else if (ndef == "plate-carree" || ndef == "plate-carre")
                        to_try = "epsg:32663";
                    else if (ndef == "moon")
                        to_try = "+proj=longlat +R=1737400 +no_defs +type=crs";

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
                        // +init= is no longer supported in PROJ 7+
                        util::replaceInPlace(to_try, "+init=", "");

                        // perhaps it's an EPSG string, in which case we must lower-case it so it
                        // works on case-sensitive file systems
                        // https://github.com/pyproj4/pyproj/blob/9283f962e4792da2a7f05ba3735c1ed7f3479502/pyproj/crs/crs.py#L111
                        util::replaceInPlace(to_try, "EPSG", "epsg");
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
                    Log()->warn("Failed to create SRS from \"{}\"", def);
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
                    {
                        new_entry.proj = proj;
                        if (new_entry.proj.find("+proj=qsc") != std::string::npos)
                            new_entry.isQSC = true;
                    }

                    // geodetic PJ associated with this CRS:
                    new_entry.pj_geodetic = proj_crs_get_geodetic_crs(ctx, new_entry.pj);

                    // cahce the underlying geocentric SRS since we use it a lot
                    if (new_entry.horiz_crs_type != PJ_TYPE_GEOCENTRIC_CRS)
                    {
                        if (new_entry.pj_geodetic)
                        {
                            // cache a full geodetic SRS:
                            new_entry.geodeticSRS = SRS(proj_as_proj_string(ctx, new_entry.pj_geodetic, PJ_PROJ_5, nullptr));

                            // and cache the corresponding geocentric SRS:
                            std::string proj = proj_as_proj_string(ctx, new_entry.pj_geodetic, PJ_PROJ_5, nullptr);
                            util::replaceInPlace(proj, "+proj=longlat", "+proj=geocent");
                            new_entry.geocentricSRS = SRS(proj);
                        }
                    }

                    else
                    {
                        // A bit hacky but it works.
                        // We do this because proj_crs_get_geodetic_crs() on a geocentric CRS
                        // just returns the same geocentric CRS. Is that a bug in proj? (checkme)
                        std::string proj = new_entry.proj;
                        util::replaceInPlace(proj, "+proj=geocent", "+proj=longlat");
                        new_entry.geodeticSRS = SRS(proj);
                    }

                    // Attempt to calculate the legal bounds for this SRS.
                    // Well known stuff:
                    if (new_entry.horiz_crs_type == PJ_TYPE_GEOGRAPHIC_2D_CRS || new_entry.horiz_crs_type == PJ_TYPE_GEOGRAPHIC_3D_CRS)
                    {
                        new_entry.bounds = Box(-180, -90, 180, 90);
                    }

                    else if (new_entry.horiz_crs_type != PJ_TYPE_GEOCENTRIC_CRS)
                    {
                        if (contains(new_entry.proj, "proj=utm"))
                        {
                            if (contains(new_entry.proj, "+south"))
                                new_entry.bounds = Box(166000, 1116915, 834000, 10000000);
                            else
                                new_entry.bounds = Box(166000, 0, 834000, 9330000);
                        }

                        else if (contains(new_entry.proj, "proj=merc"))
                        {
                            new_entry.bounds = Box(-20037508.34278925, -20037508.34278925, 20037508.34278925, 20037508.34278925);
                        }

                        else if (contains(new_entry.proj, "proj=qsc"))
                        {
                            new_entry.bounds = Box(-6378137, -6378137, 6378137, 6378137);
                        }
                    }

                    // no good? try querying it instead:
                    if (!new_entry.bounds.has_value())
                    {
                        double west_lon, south_lat, east_lon, north_lat;
                        if (proj_get_area_of_use(ctx, new_entry.pj, &west_lon, &south_lat, &east_lon, &north_lat, nullptr) &&
                            west_lon > -1000)
                        {
                            // always returns lat/long, so transform back to this srs
                            std::string geo_def = proj_as_proj_string(ctx, new_entry.pj_geodetic, PJ_PROJ_5, nullptr);
                            auto xform = get_or_create_operation(geo_def, def); // don't call proj_destroy on this
                            PJ_COORD LL = proj_trans(xform, PJ_FWD, PJ_COORD{ west_lon, south_lat, 0.0, 0.0 });
                            PJ_COORD UR = proj_trans(xform, PJ_FWD, PJ_COORD{ east_lon, north_lat, 0.0, 0.0 });
                            new_entry.bounds = Box(LL.xyz.x, LL.xyz.y, UR.xyz.x, UR.xyz.y);
                            new_entry.geodeticBounds = Box(west_lon, south_lat, east_lon, north_lat);
                        }
                    }

                    // still need geodetic bounds?
                    if (new_entry.bounds.has_value() && !new_entry.geodeticBounds.has_value())
                    {
                        std::string geo_def = proj_as_proj_string(ctx, new_entry.pj_geodetic, PJ_PROJ_5, nullptr);
                        auto xform = get_or_create_operation(def, geo_def); // don't call proj_destroy on this
                        PJ_COORD LL = proj_trans(xform, PJ_FWD, PJ_COORD{ new_entry.bounds->xmin, new_entry.bounds->ymin, 0.0, 0.0 });
                        PJ_COORD UR = proj_trans(xform, PJ_FWD, PJ_COORD{ new_entry.bounds->xmax, new_entry.bounds->ymax, 0.0, 0.0 });
                        new_entry.geodeticBounds = Box(LL.xyz.x, LL.xyz.y, UR.xyz.x, UR.xyz.y);
                    }
                }

                if (new_entry.pj == nullptr)
                {
                    Log()->debug(LC "Failed to create SRS from definition \"{}\"; PROJ error = {}", def, new_entry.error);
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
            return get_or_create(def).horiz_crs_type;
        }

        //! fetch the ellipsoid associated with an SRS definition
        //! that was previously created
        const Ellipsoid& get_ellipsoid(const std::string& def)
        {
            return get_or_create(def).ellipsoid;
        };

        //! Get the geodetic bounds of a projection if possible
        const Box& get_geodetic_bounds(const std::string& def)
        {
            SRSEntry& entry = get_or_create(def);

            if (entry.pj == nullptr || !entry.geodeticBounds.has_value())
                return empty_box;
            else
                return entry.geodeticBounds.value();
        }

        const Box& get_bounds(const std::string& def)
        {
            SRSEntry& entry = get_or_create(def);

            if (entry.pj == nullptr || !entry.bounds.has_value())
                return empty_box;
            else
                return entry.bounds.value();
        }

        const std::string& get_wkt(const std::string& def)
        {
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
            auto ctx = threading_context();

            PJ* pj = nullptr;
            std::string proj;
            std::string error;

            // make a unique identifer for the transformation
            std::string def = firstDef + "->" + secondDef;

            auto iter = find(def);

            if (iter == end())
            {
                auto& p1_def = get_or_create(firstDef);
                auto& p2_def = get_or_create(secondDef);
                PJ* p1 = p1_def.pj;
                PJ* p2 = p2_def.pj;
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
                                //" +step +proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs +towgs84=0,0,0"
                                " +step " + std::string(proj_as_proj_string(ctx, p1_def.pj_geodetic, PJ_PROJ_5, nullptr)) +
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



std::string
SRS::projVersion()
{
    return std::to_string(PROJ_VERSION_MAJOR) + "." + std::to_string(PROJ_VERSION_MINOR);
}

SRS::SRS(std::string_view h) :
    _definition(h)
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
SRS::_establish_valid() const
{
    _valid = !_definition.empty() && g_srs_factory.get_or_create(_definition).pj != nullptr;
    return _valid.value();
}

bool
SRS::isGeodetic() const
{
    if (!valid())
        return false;

    if (!_crs_type.has_value())
    {
        _crs_type = g_srs_factory.get_horiz_crs_type(_definition);
    }

    return
        (PJ_TYPE)_crs_type.value() == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
        (PJ_TYPE)_crs_type.value() == PJ_TYPE_GEOGRAPHIC_3D_CRS;
}

bool
SRS::isGeocentric() const
{
    if (!valid())
        return false;

    if (!_crs_type.has_value())
    {
        _crs_type = (int)g_srs_factory.get_horiz_crs_type(_definition);
    }

    return (PJ_TYPE)_crs_type.value() == PJ_TYPE_GEOCENTRIC_CRS;
}

bool
SRS::isProjected() const
{
    if (!valid())
        return false;

    if (!_crs_type.has_value())
    {
        _crs_type = (int)g_srs_factory.get_horiz_crs_type(_definition);
    }

    return (PJ_TYPE)_crs_type.value() == PJ_TYPE_PROJECTED_CRS;
}

bool
SRS::isQSC() const
{
    if (!valid())
        return false;

    return g_srs_factory.get_or_create(definition()).isQSC;
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
        isGeodetic() ? PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS :
        PJ_COMP_EQUIVALENT;

    return proj_is_equivalent_to_with_ctx(
        g_srs_factory.threading_context(), pj1, pj2, criterion);
}

bool
SRS::horizontallyEquivalentTo(const SRS& rhs) const
{
    if (definition().empty() || rhs.definition().empty())
        return false;

    if (isGeodetic() && rhs.isGeodetic() && ellipsoid() == rhs.ellipsoid())
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
    PJ* lhs_horiz_pj = nullptr;
    PJ* rhs_horiz_pj = nullptr;

    lhs_horiz_pj = lhs_entry.crs_type == PJ_TYPE_COMPOUND_CRS ?
        proj_crs_get_sub_crs(g_srs_factory.threading_context(), pj1, 0) : pj1;

    rhs_horiz_pj = rhs_entry.crs_type == PJ_TYPE_COMPOUND_CRS ?
        proj_crs_get_sub_crs(g_srs_factory.threading_context(), pj2, 0) : pj2;

    PJ_COMPARISON_CRITERION criterion =
        isGeodetic() ? PJ_COMP_EQUIVALENT_EXCEPT_AXIS_ORDER_GEOGCRS :
        PJ_COMP_EQUIVALENT;

    return proj_is_equivalent_to_with_ctx(
        g_srs_factory.threading_context(), lhs_horiz_pj, rhs_horiz_pj, criterion);
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

const Box&
SRS::geodeticBounds() const
{
    return g_srs_factory.get_geodetic_bounds(definition());
}

SRSOperation
SRS::to(const SRS& rhs) const
{
    return SRSOperation(*this, rhs);
}

const SRS&
SRS::geodeticSRS() const
{
    return isGeodetic() ? *this : g_srs_factory.get_or_create(_definition).geodeticSRS;
}

const SRS&
SRS::geocentricSRS() const
{
    return isGeocentric() ? *this : g_srs_factory.get_or_create(_definition).geocentricSRS;
}

glm::dmat4
SRS::topocentricToWorldMatrix(const glm::dvec3& origin) const
{
    if (!valid())
        return { };

    if (isGeodetic())
    {
        auto& ellip = ellipsoid();
        auto ecef = ellip.geodeticToGeocentric(origin);
        return ellipsoid().topocentricToGeocentricMatrix(ecef);
    }
    else if (isGeocentric())
    {
        return ellipsoid().topocentricToGeocentricMatrix(origin);
    }
    else // projected
    {
        return glm::translate(glm::dmat4(1.0), origin);
    }
}

double
SRS::transformUnits(double input, const SRS& inSRS, const SRS& outSRS, const Angle& latitude)
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
SRS::transformUnits(const Distance& distance, const SRS& outSRS, const Angle& latitude)
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

double
SRS::transformDistance(const Distance& input, const Units& outputUnits, const Angle& referenceLatitude) const
{
    auto inputUnits = input.units();

    if (inputUnits.isAngle() && outputUnits.isLinear())
    {
        auto meters = ellipsoid().longitudinalDegreesToMeters(input.as(Units::DEGREES), referenceLatitude.as(Units::DEGREES));
        return Units::convert(Units::METERS, outputUnits, meters);
    }
    else if (inputUnits.isLinear() && outputUnits.isAngle())
    {
        auto degrees = ellipsoid().metersToLongitudinalDegrees(input.as(Units::METERS), referenceLatitude.as(Units::DEGREES));
        return Units::convert(Units::DEGREES, outputUnits, degrees);
    }
    else
    {
        return input.as(outputUnits);
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


SRSOperation::SRSOperation(const SRS& from, const SRS& to) :
    _from(from),
    _to(to)
{
    _nop = (_from == _to);
    if (_from.valid() && _to.valid())
    {
        _handle = (void*)g_srs_factory.get_or_create_operation(_from.definition(), _to.definition());
    }
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

bool
SRSOperation::transformBoundsToMBR(
    double& in_out_xmin, double& in_out_ymin,
    double& in_out_xmax, double& in_out_ymax) const
{
    if (!valid() && _nop)
        return false;

    Box input(in_out_xmin, in_out_ymin, in_out_xmax, in_out_ymax);

    // first we need to clamp the inputs to the valid bounds of the output SRS.
    auto& geodetic_bounds = _to.geodeticBounds();
    if (geodetic_bounds.valid())
    {
        // start by transforming them to Long/Lat.
        SRSOperation from_to_geo(_from, _from.geodeticSRS());
        from_to_geo.transform(input.xmin, input.ymin);
        from_to_geo.transform(input.xmax, input.ymax);

        // then clamp them to the geodetic bounds of the target SRS.
        // this will return true if clamping took place.
        if (geodetic_bounds.clamp(input))
        {
            // now transform them back to the oroginal SRS.
            from_to_geo.inverse(input.xmin, input.ymin);
            from_to_geo.inverse(input.xmax, input.ymax);
        }
        else
        {
            input = Box(in_out_xmin, in_out_ymin, in_out_xmax, in_out_ymax);
        }
    }

    double height = input.ymax - input.ymin;
    double width = input.xmax - input.xmin;

    const unsigned int numSamples = 5;

    // 25 = 5 + numSamples * 4
    std::array<glm::dvec3, 25> v;
    std::size_t ptr = 0;

    // first point is a centroid. This we will use to make sure none of the corner points
    // wraps around if the target SRS is geographic.
    v[ptr++] = glm::dvec3(in_out_xmin + width * 0.5, in_out_ymin + height * 0.5, 0); // centroid.

    // add the four corners
    v[ptr++] = glm::dvec3(input.xmin, input.ymin, 0); // ll
    v[ptr++] = glm::dvec3(input.xmin, input.ymax, 0); // ul
    v[ptr++] = glm::dvec3(input.xmax, input.ymax, 0); // ur
    v[ptr++] = glm::dvec3(input.xmax, input.ymin, 0); // lr

    // We also sample along the edges of the bounding box and include them in the 
    // MBR computation in case you are dealing with a projection that will cause the edges
    // of the bounding box to be expanded.  This was first noticed when dealing with converting
    // Hotline Oblique Mercator to WGS84

    double dWidth = width / (numSamples - 1);
    double dHeight = height / (numSamples - 1);

    // Left edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v[ptr++] = glm::dvec3(input.xmin, input.ymin + dHeight * (double)i, 0);
    }

    // Right edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v[ptr++] = glm::dvec3(input.xmax, input.ymin + dHeight * (double)i, 0);
    }

    // Top edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v[ptr++] = glm::dvec3(input.xmin + dWidth * (double)i, input.ymax, 0);
    }

    // Bottom edge
    for (unsigned int i = 0; i < numSamples; i++)
    {
        v[ptr++] = glm::dvec3(input.xmin + dWidth * (double)i, input.ymin, 0);
    }

    if (transformRange(v.begin(), v.end()))
    {
        in_out_xmin = DBL_MAX;
        in_out_ymin = DBL_MAX;
        in_out_xmax = -DBL_MAX;
        in_out_ymax = -DBL_MAX;

        // For a geodetic target, make sure the new extents contain the centroid
        // because they might have wrapped around or run into a precision failure.
        // v[0]=centroid, v[1]=LL, v[2]=UL, v[3]=UR, v[4]=LR
        if (_to.isGeodetic())
        {
            if (v[1].x > v[0].x || v[2].x > v[0].x) in_out_xmin = -180.0;
            if (v[3].x < v[0].x || v[4].x < v[0].x) in_out_xmax = 180.0;
        }

        // enforce an MBR:
        for (auto& p : v)
        {
            in_out_xmin = std::min(p.x, in_out_xmin);
            in_out_ymin = std::min(p.y, in_out_ymin);
            in_out_xmax = std::max(p.x, in_out_xmax);
            in_out_ymax = std::max(p.y, in_out_ymax);
        }

        return true;
    }

    return false;
}

bool
SRSOperation::clamp(double& x, double& y) const
{
    auto& gb = _to.geodeticBounds();
    if (!gb.valid())
        return false;

    glm::dvec3 temp(x, y, 0);

    SRSOperation geo;
    if (!_from.isGeodetic())
    {
        geo = _from.to(_from.geodeticSRS());
        geo.transform(temp.x, temp.y);
    }

    temp.x = std::max(gb.xmin, std::min(gb.xmax, temp.x));
    temp.y = std::max(gb.ymin, std::min(gb.ymax, temp.y));

    if (geo.valid())
    {
        geo.inverse(temp.x, temp.y);
    }

    bool clamped = (x != temp.x) || (y != temp.y);
    x = temp.x, y = temp.y;
    return clamped;
}

std::string
SRSOperation::string() const
{
    return std::string(
        proj_as_proj_string(g_pj_thread_local_context, (PJ*)_handle, PJ_PROJ_5, nullptr));
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
