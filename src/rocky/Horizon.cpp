/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Horizon.h"

using namespace ROCKY_NAMESPACE;

Horizon::Horizon() :
    _valid(false),
    _VCmag(0), _VCmag2(0), _VHmag2(0), _coneCos(0), _coneTan(0), _minVCmag(0), _minHAE(0)
{
    setEllipsoid(Ellipsoid());
}

Horizon::Horizon(const Ellipsoid& e) :
    _valid(false),
    _VCmag(0), _VCmag2(0), _VHmag2(0), _coneCos(0), _coneTan(0), _minVCmag(0), _minHAE(0)
{
    setEllipsoid(e);
}

void
Horizon::setEllipsoid(const Ellipsoid& em)
{
    _em = em;

    _scaleInv = dvec3(
        em.semiMajorAxis(),
        em.semiMajorAxis(),
        em.semiMinorAxis());

    _scale = dvec3(
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMinorAxis());

    _minHAE = 500.0;
    _minVCmag = 1.0 + glm::length(_scale * _minHAE);

    // just so we don't have gargabe values
    setEye(dvec3(1e7, 0, 0));

    _valid = true;
}

void
Horizon::setMinHAE(double value)
{
    _minHAE = value;
    _minVCmag = 1.0 + glm::length(_scale * _minHAE);
}

bool
Horizon::setEye(const dvec3& eye)
{
    if (eye == _eye)
        return false;

    _eye = eye;
    _eyeUnit = glm::normalize(eye);

    _VC = dvec3(
        -_eye.x * _scale.x,
        -_eye.y * _scale.y,
        -_eye.z * _scale.z);  // viewer->center (scaled)
    _VCmag = std::max((double)glm::length(_VC), _minVCmag);      // clamped to the min HAE
    _VCmag2 = _VCmag * _VCmag;
    _VHmag2 = _VCmag2 - 1.0;  // viewer->horizon line (scaled)

    double VPmag = _VCmag - 1.0 / _VCmag; // viewer->horizon plane dist (scaled)
    double VHmag = sqrtf(_VHmag2);

    _coneCos = VPmag / VHmag; // cos of half-angle of horizon cone
    _coneTan = tan(acos(_coneCos));

    return true;
}

double
Horizon::getRadius() const
{
    return glm::length(_eyeUnit.x * _scaleInv);
}

bool
Horizon::isVisible(const dvec3& target, double radius) const
{
    if (_valid == false || radius >= _scaleInv.x || radius >= _scaleInv.y || radius >= _scaleInv.z)
        return true;

    // First check the object against the horizon plane, a plane that intersects the
    // ellipsoid, whose normal is the vector from the eyepoint to the center of the
    // ellipsoid.
    // ref: https://cesiumjs.org/2013/04/25/Horizon-culling/

    // Viewer-to-target vector
    // move the target closer to the horizon plane by "radius"
    // and transform into unit space
    dvec3 VTplusR = ((target + _eyeUnit * radius) - _eye) * _scale;

    // If the target is above the eye, it's visible
    double VTdotVC = glm::dot(VTplusR, _VC);
    if (VTdotVC <= 0.0)
    {
        return true;
    }

    // If the eye is below the ellipsoid, but the target is below the eye
    // (since the above test failed) the target is occluded.
    // NOTE: it might be better instead to check for a maximum distance from
    // the eyepoint instead.
    if (_VCmag < 0.0)
    {
        return false;
    }

    // Now we know that the eye is above the ellipsoid, so there is a valid horizon plane.
    // If the point is in front of that horizon plane, it's visible and we're done
    if (VTdotVC <= _VHmag2)
    {
        return true;
    }

    // The sphere is completely behind the horizon plane. So now intersect the
    // sphere with the horizon cone, a cone eminating from the eyepoint along the
    // eye->center vetor. If the sphere is entirely within the cone, it is occluded
    // by the spheroid (not ellipsoid, sorry)
    // ref: http://www.cbloom.com/3d/techdocs/culling.txt
    dvec3 VT = (target - _eye) * _scale;
    double radius_scaled = glm::distance(VT, VTplusR);

    auto VTmag2 = glm::dot(VT, VT);

    double a = glm::dot(VT, -_eyeUnit);

    // early out for a point (per the paper, but doesn't work)
    //if (radius == 0.0)
    //    return (a * a) > VTmag2 * (_coneCos * _coneCos);

    double b = a * _coneTan;
    double c = sqrt(VTmag2 - a * a);
    double d = c - b;
    double e = d * _coneCos;

    if (e > -radius_scaled)
    {
        // sphere is at least partially outside the cone (visible)
        return true;
    }

    // occluded.
    return false;
}

#if 0
bool
Horizon::getPlane(osg::Plane& out_plane) const
{
    // calculate scaled distance from center to viewer:
    if (_valid == false || _VCmag2 == 0.0)
        return false;

    double PCmag;
    if (_VCmag > 0.0)
    {
        // eyepoint is above ellipsoid? Calculate scaled distance from center to horizon plane
        PCmag = 1.0 / _VCmag;
    }
    else
    {
        // eyepoint is below the ellipsoid? plane passes through the eyepoint.
        PCmag = _VCmag;
    }

    osg::Vec3d pcWorld = osg::componentMultiply(_eyeUnit*PCmag, _scaleInv);
    double dist = glm::length(pcWorld);

    // compute a new clip plane:
    out_plane.set(_eyeUnit, -dist);
    return true;
}
#endif

double
Horizon::getDistanceToVisibleHorizon() const
{
#if 1
    return glm::length(_scaleInv * sqrt(_VHmag2));
#else
    double eyeLen = glm::length(_eye);

    dvec3 geodetic;

    geodetic = _em.geocentricToGeodetic(_eye);
    double hasl = geodetic.z;

    // limit it:
    hasl = std::max(hasl, 100.0);

    double radius = eyeLen - hasl;
    return sqrt(2.0*radius*hasl + hasl * hasl);
#endif
}
