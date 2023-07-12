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

GeoPoint&
GeoPoint::operator=(GeoPoint&& rhs)
{
    _srs = rhs._srs;
    x = rhs.x, y = rhs.y, z = rhs.z;
    //_p = rhs._p;
    rhs._srs = { }; // invalidate rhs
    return *this;
}

GeoPoint::GeoPoint(const SRS& srs, double in_x, double in_y) :
    _srs(srs),
    x(in_x), y(in_y), z(0.0)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& srs, double in_x, double in_y, double in_z) :
    _srs(srs),
    x(in_x), y(in_y), z(in_z)
{
    //nop
}

#if 0
GeoPoint::GeoPoint(const SRS& srs, const dvec3& xyz) :
    _srs(srs),
    _p(xyz)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& srs, const fvec3& xyz) :
    _srs(srs),
    _p(xyz)
{
    //nop
}
#endif

GeoPoint::GeoPoint(const SRS& srs) :
    _srs(srs),
    x(0.0), y(0.0), z(0.0)
{
    //nop
}

bool
GeoPoint::transform(const SRS& outSRS, GeoPoint& output) const
{
    if (valid() && outSRS.valid())
    {
        double xyz[3] = { x, y, z };
        //ec3 out;
        if (_srs.to(outSRS).transform(xyz, xyz))
        {
            output = GeoPoint(outSRS, xyz[0], xyz[1], xyz[2]);
            return true;
        }
    }
    return false;
}

bool
GeoPoint::transformInPlace(const SRS& srs)
{
    if (valid() && srs.valid())
    {
        double xyz[3] = { x, y, z };
        if (_srs.to(srs).transform(xyz, xyz))
        {
            x = xyz[0], y = xyz[1], z = xyz[2];
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

    GeoPoint p1, p2;
    auto geoSRS = srs().geoSRS();

    if (transform(geoSRS, p1) && rhs.transform(geoSRS, p2))
    {
        return Distance(
            srs().ellipsoid().geodesicGroundDistance(
                glm::dvec3(p1.x, p1.y, p1.z),
                glm::dvec3(p2.x, p2.y, p2.z)),
            Units::METERS);
    }
    else return { };
}


double
GeoPoint::distanceTo(const GeoPoint& rhs) const
{
    // @deprecated, because this method is ambiguous.

    if (srs().isProjected() && rhs.srs().isProjected())
    {
        if (srs().isEquivalentTo(rhs.srs()))
        {
            return glm::distance(glm::dvec3{ x,y,z }, glm::dvec3{ rhs.x, rhs.y, rhs.z });
        }
        else
        {
            GeoPoint rhsT;
            rhs.transform(srs(), rhsT);
            return glm::distance(glm::dvec3{ x,y,z }, glm::dvec3{ rhsT.x, rhsT.y, rhsT.z });
        }
    }
    else
    {
        // https://en.wikipedia.org/wiki/Geographical_distance#Ellipsoidal-surface_formulae

        GeoPoint p1, p2;
        auto geoSRS = srs().geoSRS();

        if (!transform(geoSRS, p1))
            return 0;
        if (!rhs.transform(p1.srs(), p2))
            return 0;

        double Re = srs().ellipsoid().semiMajorAxis();
        double Rp = srs().ellipsoid().semiMinorAxis();
        double F = (Re - Rp) / Re; // flattening

        double
            lat1 = deg2rad(p1.y),
            lon1 = deg2rad(p1.x),
            lat2 = deg2rad(p2.y),
            lon2 = deg2rad(p2.x);

        double B1 = atan( (1.0-F)*tan(lat1) );
        double B2 = atan( (1.0-F)*tan(lat2) );

        double P = (B1+B2)/2.0;
        double Q = (B2-B1)/2.0;

        double G = acos(sin(B1)*sin(B2) + cos(B1)*cos(B2)*cos(fabs(lon2-lon1)));

        double 
            sinG = sin(G), 
            sinP = sin(P), sinQ = sin(Q), 
            cosP = cos(P), cosQ = cos(Q), 
            sinG2 = sin(G/2.0), cosG2 = cos(G/2.0);

        double X = (G-sinG)*((sinP*sinP*cosQ*cosQ)/(cosG2*cosG2));
        double Y = (G+sinG)*((cosP*cosP*sinQ*sinQ)/(sinG2*sinG2));

        double dist = Re*(G-(F/2.0)*(X+Y));

        // NaN could mean start/end points are the same
        return std::isnan(dist)? 0.0 : dist;
    }
}


#include "json.h"

namespace ROCKY_NAMESPACE
{
    void to_json(json& j, const GeoPoint& obj) {
        if (obj.valid()) {
            j = json::object();
            if (obj.srs().isGeodetic()) {
                set(j, "lat", obj.y);
                set(j, "long", obj.x);
            }
            else {
                set(j, "x", obj.x);
                set(j, "y", obj.y);
            }
            set(j, "z", obj.z);
            set(j, "srs", obj.srs());
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
