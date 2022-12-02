#include "GeoPoint.h"
#include "Metrics.h"
#include "Math.h"
#include "TerrainResolver.h"
#include "StringUtils.h"

using namespace rocky;
using namespace rocky::util;

#undef  LC
#define LC "[GeoPoint] "

GeoPoint GeoPoint::INVALID;

GeoPoint::GeoPoint(
    shared_ptr<SRS> srs,
    double x,
    double y) :

    _srs(srs),
    _p(x, y, 0.0),
    _altMode(ALTMODE_RELATIVE)
{
    //nop
}

GeoPoint::GeoPoint(
    shared_ptr<SRS> srs,
    double x,
    double y,
    double z,
    const AltitudeMode& altMode) :

    _srs(srs),
    _p(x, y, z),
    _altMode(altMode)
{
    //nop
}

GeoPoint::GeoPoint(
    shared_ptr<SRS> srs,
    const dvec3& xyz,
    const AltitudeMode& altMode) :

    _srs(srs),
    _p(xyz),
    _altMode(altMode)
{
    //nop
}

GeoPoint::GeoPoint(
    shared_ptr<SRS> srs,
    const GeoPoint& rhs) :

    _altMode(rhs._altMode)
{
     rhs.transform(srs, *this);
}

GeoPoint::GeoPoint(const GeoPoint& rhs) :
    _srs(rhs._srs.get()),
    _p(rhs._p),
    _altMode(rhs._altMode)
{
    //nop
}

GeoPoint::GeoPoint() :
    _srs(0L),
    _altMode(ALTMODE_ABSOLUTE)
{
    //nop
}

GeoPoint::GeoPoint(shared_ptr<SRS> srs) :
    _srs(srs),
    _altMode(ALTMODE_ABSOLUTE)
{
    //nop
}

GeoPoint::GeoPoint(const Config& conf, shared_ptr<SRS> srs) :
    _srs(srs),
    _altMode(ALTMODE_ABSOLUTE)
{
    conf.get( "x", _p.x );
    conf.get( "y", _p.y );
    conf.get( "z", _p.z );
    conf.get( "alt", _p.z );
    conf.get( "hat", _p.z ); // height above terrain (relative)

    if ( !_srs && conf.hasValue("srs") )
        _srs = SRS::get( conf.value("srs"), conf.value("vdatum") );

    if ( conf.hasValue("lat") && (!_srs || _srs->isGeographic()) )
    {
        conf.get( "lat", _p.y );
        if ( !_srs ) 
            _srs = SRS::get("wgs84");
    }
    if ( conf.hasValue("long") && (!_srs || _srs->isGeographic()))
    {
        conf.get("long", _p.x);
        if ( !_srs ) 
            _srs = SRS::get("wgs84");
    }

    if ( conf.hasValue("mode") )
    {
        conf.get( "mode", "relative",            _altMode, ALTMODE_RELATIVE );
        conf.get( "mode", "relative_to_terrain", _altMode, ALTMODE_RELATIVE );
        conf.get( "mode", "absolute",            _altMode, ALTMODE_ABSOLUTE );
    }
    else
    {
        if (conf.hasValue("hat"))
            _altMode = ALTMODE_RELATIVE;
        else if ( conf.hasValue("alt") || conf.hasValue("z") )
            _altMode = ALTMODE_ABSOLUTE;
        else
            _altMode = ALTMODE_RELATIVE;
    }
}

Config
GeoPoint::getConfig() const
{
    Config conf;
    if ( _srs && _srs->isGeographic() )
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

    // default is absolute
    if (_altMode == ALTMODE_RELATIVE)
        conf.set("mode", "relative");

    if ( _srs )
    {
        conf.set("srs", _srs->getHorizInitString());
        if ( _srs->getVerticalDatum() )
            conf.set("vdatum", _srs->getVertInitString());
    }

    return conf;
}

void
GeoPoint::set(
    shared_ptr<SRS> srs,
    const dvec3& xyz,
    const AltitudeMode& altMode)
{
    _srs = srs;
    _p = xyz;
    _altMode = altMode;
}

void
GeoPoint::set(
    shared_ptr<SRS> srs,
    double                  x,
    double                  y,
    double                  z,
    const AltitudeMode&     altMode)
{
    _srs = srs;
    _p = dvec3(x, y, z);
    _altMode = altMode;
}

const Units&
GeoPoint::getXYUnits() const
{
    return getSRS() ? getSRS()->getUnits() : Units::DEGREES;
}

bool 
GeoPoint::operator == (const GeoPoint& rhs) const
{
    return
        valid() && rhs.valid() &&
        _p        == rhs._p        &&
        _altMode  == rhs._altMode  &&
        ((_altMode == ALTMODE_ABSOLUTE && _srs->isEquivalentTo(rhs._srs.get())) ||
         (_altMode == ALTMODE_RELATIVE && _srs->isHorizEquivalentTo(rhs._srs.get())));
}

GeoPoint
GeoPoint::transform(shared_ptr<SRS> outSRS) const
{
    if (valid() && outSRS)
    {
        dvec3 out;
        if (_altMode == ALTMODE_ABSOLUTE)
        {
            if (_srs->transform(_p, outSRS.get(), out))
                return GeoPoint(outSRS, out.x, out.y, out.z, ALTMODE_ABSOLUTE);
        }
        else // if ( _altMode == ALTMODE_RELATIVE )
        {
            if (_srs->transform2D(_p.x, _p.y, outSRS, out.x, out.y))
            {
                out.z = _p.z;
                return GeoPoint(outSRS, out.x, out.y, out.z, ALTMODE_RELATIVE);
            }
        }
    }
    return GeoPoint::INVALID;
}

bool
GeoPoint::transformInPlace(shared_ptr<SRS> srs)
{
    if ( valid() && srs )
    {
        dvec3 out;
        if ( _altMode == ALTMODE_ABSOLUTE )
        {
            if ( _srs->transform(_p, srs.get(), out) )
            {
                set(srs, out.x, out.y, out.z, ALTMODE_ABSOLUTE);
                return true;
            }
        }
        else // if ( _altMode == ALTMODE_RELATIVE )
        {
            if ( _srs->transform2D(_p.x, _p.y, srs, out.x, out.y) )
            {
                out.z = _p.z;
                set(srs, out, ALTMODE_RELATIVE);
                return true;
            }
        }
    }
    return false;
}

bool
GeoPoint::transformZ(const AltitudeMode& altMode, const TerrainResolver* terrain ) 
{
    double z;
    if ( transformZ(altMode, terrain, z) )
    {
        _p.z = z;
        _altMode = altMode;
        return true;
    }
    return false;
}

bool
GeoPoint::transformZ(const AltitudeMode& altMode, const TerrainResolver* terrain, double& out_z ) const
{
    if ( !valid() )
        return false;
    
    // already in the target mode? just return z.
    if ( _altMode == altMode ) 
    {
        out_z = z();
        return true;
    }

    if ( !terrain )
        return false;

    // convert to geographic if necessary and sample the MSL height under the point.
    double out_hamsl;
    if ( !terrain->getHeight(_srs, x(), y(), &out_hamsl) )
    {
        return false;
    }

    // convert the Z value as appropriate.
    if ( altMode == ALTMODE_RELATIVE )
    {
        out_z = z() - out_hamsl;
    }
    else // if ( altMode == ALTMODE_ABSOLUTE )
    {
        out_z = z() + out_hamsl;
    }
    return true;
}

Distance
GeoPoint::transformResolution(const Distance& resolution, const Units& outUnits) const
{
    if (!valid())
        return resolution;

    // is this just a normal transformation?
    if (resolution.getUnits().isLinear() ||
        outUnits.isAngular())
    {
        return resolution.to(outUnits);
    }

    double refLatDegrees = y();

    if (!getSRS()->isGeographic())
    {
        double refLonDegrees; // unused

        getSRS()->transform2D(
            x(), y(),
            getSRS()->getGeographicSRS(),
            refLonDegrees,
            refLatDegrees);
    }

    double d = resolution.asDistance(outUnits, refLatDegrees);
    return Distance(d, outUnits);
}

bool
GeoPoint::makeGeographic()
{
    if (!valid())
        return false;
    if (!_srs->isGeographic())
        return _srs->transform(_p, _srs->getGeographicSRS(), _p);
    return true;
}

bool
GeoPoint::transform(shared_ptr<SRS> outSRS, GeoPoint& output) const
{
    output = transform(outSRS);
    return output.valid();
}

bool
GeoPoint::toWorld( dvec3& out_world ) const
{
    if ( !valid() )
    {
        ROCKY_WARN << LC << "Called toWorld() on an invalid point" << std::endl;
        return false;
    }
    if ( _altMode != ALTMODE_ABSOLUTE )
    {
        ROCKY_WARN << LC << "ILLEGAL: called GeoPoint::toWorld with AltitudeMode = RELATIVE_TO_TERRAIN" << std::endl;
        return false;
    }
    return _srs->transformToWorld( _p, out_world );
}

bool
GeoPoint::toWorld( dvec3& out_world, const TerrainResolver* terrain ) const
{
    if ( !valid() )
    {
        ROCKY_WARN << LC << "Called toWorld() on an invalid point" << std::endl;
        return false;
    }
    if ( _altMode == ALTMODE_ABSOLUTE )
    {
        return _srs->transformToWorld( _p, out_world );
    }
    else if ( terrain != 0L )
    {
        GeoPoint absPoint = *this;
        if (!absPoint.makeAbsolute( terrain ))
        {
            return false;            
        }
        return absPoint.toWorld( out_world );
    }
    else
    {
        ROCKY_WARN << LC << "ILLEGAL: called GeoPoint::toWorld with AltitudeMode = RELATIVE_TO_TERRAIN" << std::endl;
        return false;
    }
}


bool
GeoPoint::fromWorld(shared_ptr<SRS> srs, const dvec3& world)
{
    if ( srs )
    {
        dvec3 p;
        if ( srs->transformFromWorld(world, p) )
        {
            set( srs, p, ALTMODE_ABSOLUTE );
            return true;
        }
    }
    return false;
}

bool
GeoPoint::createLocalToWorld( dmat4& out_l2w ) const
{
    if ( !valid() ) return false;
    bool result = _srs->createLocalToWorld( _p, out_l2w );
    if ( _altMode != ALTMODE_ABSOLUTE )
    {
        ROCKY_DEBUG << LC << "ILLEGAL: called GeoPoint::createLocalToWorld with AltitudeMode = RELATIVE_TO_TERRAIN" << std::endl;
        return false;
    }
    return result;
}

bool
GeoPoint::createWorldToLocal( dmat4& out_w2l ) const
{
    if ( !valid() ) return false;
    bool result = _srs->createWorldToLocal( _p, out_w2l );
    if ( _altMode != ALTMODE_ABSOLUTE )
    {
        ROCKY_DEBUG << LC << "ILLEGAL: called GeoPoint::createWorldToLocal with AltitudeMode = RELATIVE_TO_TERRAIN" << std::endl;
        return false;
    }
    return result;
}

GeoPoint
GeoPoint::toLocalTangentPlane() const
{
    if (!valid())
        return GeoPoint::INVALID;

    return transform(getSRS()->createTangentPlaneSRS(dvec3()));
}

bool
GeoPoint::createWorldUpVector( dvec3& out_up ) const
{
    if ( !valid() ) return false;

    if ( _srs->isProjected() )
    {
        out_up = dvec3(0, 0, 1);
        return true;
    }
    else if (_srs->isGeographic())
    {
        double coslon = cos(deg2rad(x()));
        double coslat = cos(deg2rad(y()));
        double sinlon = sin(deg2rad(x()));
        double sinlat = sin(deg2rad(y()));

        out_up = dvec3(coslon*coslat, sinlon*coslat, sinlat);
        return true;
    }
    else
    {
        dvec3 ecef;
        if ( this->toWorld( ecef ) )
        {
            out_up = _srs->getEllipsoid().geocentricToUpVector(ecef);
            return true;
        }
    }
    return false;
}

GeoPoint
GeoPoint::interpolate(const GeoPoint& rhs, double t) const
{
    if (t == 0.0)
        return *this;
    else if (t == 1.0)
        return rhs;

    GeoPoint result;

    GeoPoint to = rhs.transform(getSRS());

    if (getSRS()->isProjected())
    {
        dvec3 w1, w2;
        toWorld(w1);
        rhs.toWorld(w2);

        dvec3 r(
            w1.x + (w2.x - w1.x)*t,
            w1.y + (w2.y - w1.y)*t,
            w1.z + (w2.z - w1.z)*t);

        result.fromWorld(getSRS(), r);
    }

    else // geographic
    {
        dvec3 output;

        getSRS()->getEllipsoid().geodesicInterpolate(
            dvec3(),
            to.to_dvec3(),
            t,
            output);

        result.set(
            getSRS(),
            output,
            ALTMODE_ABSOLUTE);
    }

    return result;
}

Distance
GeoPoint::geodesicDistanceTo(const GeoPoint& rhs) const
{
    // Transform both points to lat/long and do a great circle measurement.
    // https://en.wikipedia.org/wiki/Geographical_distance#Ellipsoidal-surface_formulae

    GeoPoint p1 = transform(getSRS()->getGeographicSRS());
    GeoPoint p2 = rhs.transform(p1.getSRS());

    return Distance(
        getSRS()->getEllipsoid().geodesicDistance(
            dvec2(p1.x(), p1.y()),
            dvec2(p2.x(), p2.y())),
        Units::METERS);
}


double
GeoPoint::distanceTo(const GeoPoint& rhs) const
{
    // @deprecated, because this method is ambiguous.

    if ( getSRS()->isProjected() && rhs.getSRS()->isProjected() )
    {
        if (getSRS()->isEquivalentTo(rhs.getSRS()))
        {
            return (_p - rhs._p).length();
        }
        else
        {
            GeoPoint rhsT = rhs.transform(getSRS());
            return (_p - rhsT._p).length();
        }
    }
    else
    {
        // https://en.wikipedia.org/wiki/Geographical_distance#Ellipsoidal-surface_formulae

        GeoPoint p1 = transform( getSRS()->getGeographicSRS() );
        GeoPoint p2 = rhs.transform( p1.getSRS() );

        double Re = getSRS()->getEllipsoid().getRadiusEquator();
        double Rp = getSRS()->getEllipsoid().getRadiusPolar();
        double F  = (Re-Rp)/Re; // flattening

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
    std::stringstream buf;
    buf << "x=" << x() << ", y=" << y() << ", z=" << z() << "; m=" <<
        (_altMode == ALTMODE_ABSOLUTE ? "abs" : "rel");
    return buf.str();
}
