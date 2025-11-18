#include "GeoCircle.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;

#define LC "[GeoCircle] "

#undef EPSILON
#define EPSILON 1e-6

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
        glm::epsilonEqual(_radius, rhs._radius, EPSILON);
}

GeoCircle
GeoCircle::transform(const SRS& srs) const
{
    GeoCircle result;
    result._radius = _radius;
    result._center = center().transform(srs);
    return result;
}

bool 
GeoCircle::intersects( const GeoCircle& rhs ) const
{
    if ( !valid() || !rhs.valid() )
        return false;

    if ( !srs().horizontallyEquivalentTo( rhs.srs() ) )
    {
        return intersects(rhs.transform(srs()));
    }
    else
    {
        if ( srs().isProjected() )
        {
            auto vec = glm::dvec2(center().x, center().y) - glm::dvec2(rhs.center().x, rhs.center().y);
            return glm::dot(vec, vec) <= (radius() + rhs.radius())*(radius() + rhs.radius());
        }
        else // if ( isGeodetic() )
        {
            GeoPoint p0(srs(), center().x, center().y, 0.0);
            GeoPoint p1(srs(), rhs.center().x, rhs.center().y, 0.0);
            return p0.geodesicDistanceTo(p1) <= Distance(radius() + rhs.radius());
        }
    }
}
