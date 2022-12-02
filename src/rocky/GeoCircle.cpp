#include "GeoCircle.h"
#include "Math.h"

using namespace rocky;

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
        equivalent(_radius, rhs._radius);
}

GeoCircle
GeoCircle::transform(shared_ptr<SRS> srs) const
{
    return GeoCircle(
        center().transform( srs ),
        radius() );
}

bool
GeoCircle::transform(
    shared_ptr<SRS> srs,
    GeoCircle& output) const
{
    output._radius = _radius;
    return center().transform(srs, output._center);
}

bool 
GeoCircle::intersects( const GeoCircle& rhs ) const
{
    if ( !valid() || !rhs.valid() )
        return false;

    if ( !getSRS()->isHorizEquivalentTo( rhs.getSRS() ) )
    {
        return intersects( rhs.transform(getSRS()) );
    }
    else
    {
        if ( getSRS()->isProjected() )
        {
            dvec2 vec = dvec2(center().x(), center().y()) - dvec2(rhs.center().x(), rhs.center().y());
            return length_squared(vec) <= (radius() + rhs.radius())*(radius() + rhs.radius());
        }
        else // if ( isGeographic() )
        {
            GeoPoint p0(getSRS(), dvec3(center().x(), center().y(), 0.0));
            GeoPoint p1(getSRS(), dvec3(rhs.center().x(), rhs.center().y(), 0.0));
            return p0.geodesicDistanceTo(p1) <= (radius() + rhs.radius());
            //dvec3 p0( _center.x(), _center.y(), 0.0 );
            //dvec3 p1( rhs.getCenter().x(), rhs.getCenter().y(), 0.0 );
            //return GeoMath::distance( p0, p1, getSRS() ) <= (_radius + rhs.getRadius());
        }
    }
}
