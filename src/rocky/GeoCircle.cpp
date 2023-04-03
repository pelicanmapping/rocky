#include "GeoCircle.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;

#define LC "[GeoCircle] "

GeoCircle GeoCircle::INVALID = GeoCircle();


GeoCircle::GeoCircle() :
    _center(GeoPoint::INVALID),
    _radius(-1.0)
{
    //nop
}

GeoCircle::GeoCircle(const GeoCircle& rhs) :
    _center(rhs._center),
    _radius(rhs._radius)
{
    //nop
}

GeoCircle::GeoCircle(const GeoPoint& center,
    double          radius) :
    _center(center),
    _radius(radius)
{
    //nop
}

bool
GeoCircle::operator == ( const GeoCircle& rhs ) const
{
    return
        _center == rhs._center &&
        equiv(_radius, rhs._radius);
}

bool
GeoCircle::transform(const SRS& srs, GeoCircle& output) const
{
    output._radius = _radius;
    center().transform(srs, output._center);
    return true;
}

bool 
GeoCircle::intersects( const GeoCircle& rhs ) const
{
    if ( !valid() || !rhs.valid() )
        return false;

    if ( !srs().isHorizEquivalentTo( rhs.srs() ) )
    {
        GeoCircle c;
        return rhs.transform(SRS(), c) && intersects(c);
    }
    else
    {
        if ( srs().isProjected() )
        {
            dvec2 vec = dvec2(center().x(), center().y()) - dvec2(rhs.center().x(), rhs.center().y());
            return lengthSquared(vec) <= (radius() + rhs.radius())*(radius() + rhs.radius());
        }
        else // if ( isGeodetic() )
        {
            GeoPoint p0(srs(), dvec3(center().x(), center().y(), 0.0));
            GeoPoint p1(srs(), dvec3(rhs.center().x(), rhs.center().y(), 0.0));
            return p0.geodesicDistanceTo(p1) <= (radius() + rhs.radius());
            //dvec3 p0( _center.x(), _center.y(), 0.0 );
            //dvec3 p1( rhs.getCenter().x(), rhs.getCenter().y(), 0.0 );
            //return GeoMath::distance( p0, p1, srs() ) <= (_radius + rhs.getRadius());
        }
    }
}
