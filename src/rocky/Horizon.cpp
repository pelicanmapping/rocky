/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Horizon.h"

using namespace ROCKY_NAMESPACE;

Horizon::Horizon()
{
    setEllipsoid(Ellipsoid());
}

Horizon::Horizon(const Ellipsoid& e)
{
    setEllipsoid(e);
}

void
Horizon::setEllipsoid(const Ellipsoid& em)
{
    _em = em;

    _scaleInv = glm::dvec3(
        em.semiMajorAxis(),
        em.semiMajorAxis(),
        em.semiMinorAxis());

    _scale = glm::dvec3(
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMajorAxis(),
        1.0 / em.semiMinorAxis());

    _minHAE = 500.0;
    _minVCmag = 1.0 + glm::length(_scale * _minHAE);

    // just so we don't have gargabe values
    setEye(glm::dvec3(1e7, 0, 0));

    _valid = true;
}

bool
Horizon::setEye(const glm::dvec3& eye, bool isOrtho)
{
    if (eye == _eye)
        return false;

    _eye = eye;
    _eyeUnit = glm::normalize(eye);
    _orthographic = isOrtho;

    if (!_orthographic)
    {
        _VC = glm::dvec3(
            -_eye.x * _scale.x,
            -_eye.y * _scale.y,
            -_eye.z * _scale.z);  // viewer->center (scaled)
        _VCmag = std::max((double)glm::length(_VC), _minVCmag); // clamped to the min HAE
        _VCmag2 = _VCmag * _VCmag;
        _VHmag2 = _VCmag2 - 1.0;  // viewer->horizon line (scaled)

        double VPmag = _VCmag - 1.0 / _VCmag; // viewer->horizon plane dist (scaled)
        double VHmag = sqrt(_VHmag2);

        _coneCos = VPmag / VHmag; // cos of half-angle of horizon cone
        _coneTan = tan(acos(_coneCos));
    }

    return true;
}

double
Horizon::getRadius() const
{
    return glm::length(_eyeUnit.x * _scaleInv);
}

bool
Horizon::isVisible(double x, double y, double z, double radius) const
{
    if (_valid == false || radius >= _scaleInv.x || radius >= _scaleInv.y || radius >= _scaleInv.z)
        return true;

    glm::dvec3 target(x, y, z);

    if (_orthographic)
    {
        glm::dvec3 CT(target * _scale);
        auto CTmag = glm::length(CT);
        CT /= CTmag; // normalize

        double cos_a = glm::dot(-_eyeUnit, CT);
        if (cos_a <= 0.0)
            return true;

        double x = CTmag * cos_a;
        double d = CTmag * CTmag - x * x;

        return d >= 1.0;
    }

    // First check the object against the horizon plane, a plane that intersects the
    // ellipsoid, whose normal is the vector from the eyepoint to the center of the
    // ellipsoid.
    // ref: https://cesiumjs.org/2013/04/25/Horizon-culling/

    glm::dvec3 VT;
    VT = (target + _eyeUnit * radius) - _eye;
    VT = VT * _scale;

    double VTdotVC = glm::dot(VT, _VC);
    if (VTdotVC <= 0.0)
        return true;

    if (_VCmag < 0.0)
        return false;

    if (VTdotVC <= _VHmag2)
        return true;

    VT = target - _eye;
    double a = glm::dot(VT, -_eyeUnit);
    double b = a * _coneTan;
    double c = sqrt(glm::dot(VT, VT) - a * a);
    double d = c - b;
    double e = d * _coneCos;
    if (e > -radius)
        return true;

    return false;
}

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
