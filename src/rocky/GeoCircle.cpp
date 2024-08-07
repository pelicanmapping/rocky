#include "GeoCircle.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

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

    if ( !srs().horizontallyEquivalentTo( rhs.srs() ) )
    {
        GeoCircle c;
        return rhs.transform(SRS(), c) && intersects(c);
    }
    else
    {
        if ( srs().isProjected() )
        {
            auto vec = glm::dvec2(center().x, center().y) - glm::dvec2(rhs.center().x, rhs.center().y);
            return lengthSquared(vec) <= (radius() + rhs.radius())*(radius() + rhs.radius());
        }
        else // if ( isGeodetic() )
        {
            GeoPoint p0(srs(), center().x, center().y, 0.0);
            GeoPoint p1(srs(), rhs.center().x, rhs.center().y, 0.0);
            return p0.geodesicDistanceTo(p1) <= Distance(radius() + rhs.radius());
        }
    }
}
