#include "GeoPoint.h"
#include "Math.h"
#include "Utils.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#undef  LC
#define LC "[GeoPoint] "

GeoPoint GeoPoint::INVALID;

GeoPoint::GeoPoint()
{
    //nop
}

GeoPoint::GeoPoint(const SRS& in_srs, double in_x, double in_y) :
    srs(in_srs),
    x(in_x), y(in_y), z(0.0)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& in_srs, double in_x, double in_y, double in_z) :
    srs(in_srs),
    x(in_x), y(in_y), z(in_z)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& in_srs) :
    srs(in_srs),
    x(0.0), y(0.0), z(0.0)
{
    //nop
}

GeoPoint
GeoPoint::transform(const SRS& outSRS) const
{
    GeoPoint result;
    if (valid() && outSRS.valid())
    {
        double xyz[3] = { x, y, z };
        if (srs.to(outSRS).transform(xyz, xyz))
        {
            return GeoPoint(outSRS, xyz[0], xyz[1], xyz[2]);
        }
    }
    return {};
}

bool
GeoPoint::transformInPlace(const SRS& to_srs)
{
    if (valid() && srs.valid())
    {
        double xyz[3] = { x, y, z };
        if (srs.to(to_srs).transform(xyz, xyz))
        {
            x = xyz[0], y = xyz[1], z = xyz[2];
            srs = to_srs;
            return true;
        }
    }
    return false;
}

Distance
GeoPoint::geodesicDistanceTo(const GeoPoint& rhs) const
{
    // Transform both points to lat/long and do a great circle measurement.
    // https://en.wikipedia.org/wiki/Geographical_distance#Ellipsoidal-surface_formulae

    auto geoSRS = srs.geodeticSRS();
    auto p1 = transform(geoSRS);
    auto p2 = rhs.transform(geoSRS);

    if (p1.valid() && p2.valid())
    {
        return Distance(
            srs.ellipsoid().geodesicGroundDistance(
                glm::dvec3(p1.x, p1.y, p1.z),
                glm::dvec3(p2.x, p2.y, p2.z)),
            Units::METERS);
    }
    else return { };
}

#include "json.h"

namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const GeoPoint& obj) {
        if (obj.valid()) {
            j = json::object();
            if (obj.srs.isGeodetic()) {
                set(j, "lat", obj.y);
                set(j, "long", obj.x);
            }
            else {
                set(j, "x", obj.x);
                set(j, "y", obj.y);
            }
            set(j, "z", obj.z);
            set(j, "srs", obj.srs);
        }
    }

    void from_json(const json& j, GeoPoint& obj) {
        SRS srs;
        double x = 0, y = 0, z = 0;

        get_to(j, "srs", srs);
        if (!srs.valid())
            srs = SRS::WGS84;
        if (srs.isGeodetic()) {
            get_to(j, "lat", y);
            get_to(j, "long", x);
        }
        get_to(j, "x", x);
        get_to(j, "y", y);
        get_to(j, "z", z);

        obj = GeoPoint(srs, x, y, z);
    }
}
