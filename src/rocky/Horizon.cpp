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
    _minScaleInv = std::min(_scaleInv.x, std::min(_scaleInv.y, _scaleInv.z));
    _radiusScale = std::max(_scale.x, std::max(_scale.y, _scale.z));

    _minHAE = 500.0;
    _minVCmag = 1.0 + glm::length(_scale * _minHAE);

    // just so we don't have garbage values
    _eye = glm::dvec3(0.0, 0.0, 0.0);
    setEye(glm::dvec3(1e7, 0, 0));

    _valid = true;
}

bool
Horizon::setEye(const glm::dvec3& eye, bool isOrtho)
{
    if (eye == _eye && isOrtho == _orthographic)
        return false;

    _eye = eye;
    _eyeUnit = glm::normalize(eye);
    _eyeScaled = _eye * _scale;
    _eyeDirScaled = glm::normalize(_eyeScaled);
    _orthographic = isOrtho;

    if (!_orthographic)
    {
        _VC = -_eyeScaled;  // viewer->center (scaled)
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
    double effectiveRadius = radius > 0.0 ? radius : 0.0;

    if (_valid == false || effectiveRadius >= _minScaleInv)
        return true;

    glm::dvec3 targetScaled(x * _scale.x, y * _scale.y, z * _scale.z);

    // A world-space sphere becomes an ellipsoid in scaled space. Use a sphere
    // that contains it so the horizon test remains conservative.
    double radiusScaled = effectiveRadius > 0.0 ? effectiveRadius * _radiusScale : 0.0;

    if (_orthographic)
    {
        glm::dvec3 CT(targetScaled);
        auto CTmag = glm::length(CT);
        if (CTmag <= radiusScaled)
            return true;

        CT /= CTmag; // normalize

        double cos_a = glm::dot(-_eyeDirScaled, CT);
        if (cos_a <= 0.0)
            return true;

        double x = CTmag * cos_a;
        double d = CTmag * CTmag - x * x;
        double minVisibleDistance = std::max(0.0, 1.0 - radiusScaled);

        return d >= minVisibleDistance * minVisibleDistance;
    }

    // First check the object against the horizon plane, a plane that intersects the
    // ellipsoid, whose normal is the vector from the eyepoint to the center of the
    // ellipsoid.
    // ref: https://cesiumjs.org/2013/04/25/Horizon-culling/

    glm::dvec3 VT = (targetScaled + _eyeDirScaled * radiusScaled) - _eyeScaled;

    double VTdotVC = glm::dot(VT, _VC);
    if (VTdotVC <= 0.0)
        return true;

    if (_VCmag < 0.0)
        return false;

    if (VTdotVC <= _VHmag2)
        return true;

    if (radiusScaled <= 0.0)
    {
        double VTmag2 = glm::dot(VT, VT);
        return VTmag2 > 0.0 && (VTdotVC * VTdotVC / VTmag2) <= _VHmag2;
    }

    VT = targetScaled - _eyeScaled;
    double a = glm::dot(VT, -_eyeDirScaled);
    double b = a * _coneTan;
    double c = sqrt(std::max(0.0, glm::dot(VT, VT) - a * a));
    double d = c - b;
    double e = d * _coneCos;
    if (e > -radiusScaled)
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
