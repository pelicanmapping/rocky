#include "GeoPoint.h"
#include "Math.h"
#include "Utils.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#undef  LC
#define LC "[GeoPoint] "

GeoPoint GeoPoint::INVALID;

GeoPoint::GeoPoint()
{
    //nop
}

GeoPoint::GeoPoint(const SRS& in_srs, double in_x, double in_y, double in_z) :
    glm::dvec3(in_x, in_y, in_z),
    srs(in_srs)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& in_srs) :
    glm::dvec3(0, 0, 0),
    srs(in_srs)
{
    //nop
}

GeoPoint
GeoPoint::transform(const SRS& outSRS) const
{
    GeoPoint result;
    if (valid() && outSRS.valid())
    {
        if (srs.to(outSRS).transform(*this, result))
        {
            result.srs = outSRS;
        }
    }
    return result;
}

GeoPoint&
GeoPoint::transformInPlace(const SRS& to_srs)
{
    if (valid() && srs.valid())
    {
        if (srs.to(to_srs).transform(*this, *this))
        {
            srs = to_srs;
        }
        else
        {
            srs = {}; // invalidate if it fail
        }
    }
    return *this;
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

GeoPoint
GeoPoint::interpolateTo(const GeoPoint& rhs, double t) const
{
    t = std::clamp(t, 0.0, 1.0);
    
    if (srs.isGeodetic())
    {
        return GeoPoint(srs, srs.ellipsoid().geodesicInterpolate(
            glm::dvec3(x, y, z),
            glm::dvec3(rhs.x, rhs.y, rhs.z),
            t));
    }
    else
    {
        auto vec = glm::dvec3(rhs.x - x, rhs.y - y, rhs.z - z);
        return GeoPoint(srs, x + t * vec.x, y + t * vec.y, z + t * vec.z);
    }
}

std::string
GeoPoint::string() const
{
    return std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + " (" + srs.definition() + ")";
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
