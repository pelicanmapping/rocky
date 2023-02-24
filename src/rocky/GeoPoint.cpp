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
    _p = rhs._p;
    rhs._srs = { }; // invalidate rhs
    return *this;
}

GeoPoint::GeoPoint(const SRS& srs, double x, double y) :
    _srs(srs),
    _p(x, y, 0.0)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& srs, double x, double y, double z) :
    _srs(srs),
    _p(x, y, z)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& srs, const dvec3& xyz) :
    _srs(srs),
    _p(xyz)
{
    //nop
}

GeoPoint::GeoPoint(const SRS& srs) :
    _srs(srs),
    _p(0.0, 0.0, 0.0)
{
    //nop
}

GeoPoint::GeoPoint(const Config& conf, const SRS& srs) :
    _srs(srs)
{
    conf.get( "x", _p.x );
    conf.get( "y", _p.y );
    conf.get( "z", _p.z );
    conf.get( "alt", _p.z );
    conf.get( "hat", _p.z ); // height above terrain (relative)

    if (!_srs.valid() && conf.hasValue("srs"))
        _srs = SRS(conf.value("srs"));

    if ( conf.hasValue("lat") && (!_srs.valid() || _srs.isGeographic()) )
    {
        conf.get( "lat", _p.y );
        if (!_srs.valid())
            _srs = SRS::WGS84;
    }
    if ( conf.hasValue("long") && (!_srs.valid() || _srs.isGeographic()))
    {
        conf.get("long", _p.x);
        if (!_srs.valid())
            _srs = SRS::WGS84;
    }
}

Config
GeoPoint::getConfig() const
{
    Config conf;
    if ( _srs.isGeographic() )
    {
        conf.set( "lat", _p.y );
        conf.set( "long", _p.x );
    }
    else
    {
        conf.set( "x", _p.x );
        conf.set( "y", _p.y );
    }

    conf.set("z", _p.z);

    if (_srs.valid())
    {
        conf.set("srs", _srs.definition());
    }

    return conf;
}

bool
GeoPoint::transform(const SRS& outSRS, GeoPoint& output) const
{
    if (valid() && outSRS.valid())
    {
        dvec3 out;
        if (_srs.to(outSRS).transform(_p, out))
        {
            output = GeoPoint(outSRS, out);
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
        dvec3 out;
        if (_srs.to(srs).transform(_p, out))
        {
            _p = out;
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
            srs().ellipsoid().geodesicDistance(
                dvec2(p1.x(), p1.y()),
                dvec2(p2.x(), p2.y())),
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
            return glm::distance(_p, rhs._p);
        }
        else
        {
            GeoPoint rhsT;
            rhs.transform(srs(), rhsT);
            return glm::distance(_p, rhsT._p);
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
            lat1 = deg2rad(p1.y()),
            lon1 = deg2rad(p1.x()),
            lat2 = deg2rad(p2.y()),
            lon2 = deg2rad(p2.x());

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

std::string
GeoPoint::toString() const
{
    return util::make_string() << "x=" << x() << ", y=" << y() << ", z=" << z();
}
