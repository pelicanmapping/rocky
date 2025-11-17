/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Ellipsoid.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

// https://en.wikipedia.org/wiki/World_Geodetic_System
#define WGS84_RADIUS_EQUATOR 6378137.0
#define WGS84_RADIUS_POLAR 6356752.314245

#define PI_OVER_2 (0.5*M_PI)

namespace
{
    static void applyTopocentricRotation(double latitude, double longitude, glm::dmat4& frame)
    {
        // Compute up vector
        glm::dvec3 up(cos(longitude)*cos(latitude), sin(longitude)*cos(latitude), sin(latitude));

        // Compute east vector
        glm::dvec3 east(-sin(longitude), cos(longitude), 0);

        // Compute north vector = outer product up x east
        glm::dvec3 north = glm::cross(up, east);

        frame[0][0] = east[0];
        frame[0][1] = east[1];
        frame[0][2] = east[2];

        frame[1][0] = north[0];
        frame[1][1] = north[1];
        frame[1][2] = north[2];

        frame[2][0] = up[0];
        frame[2][1] = up[1];
        frame[2][2] = up[2];
    }
}

Ellipsoid::Ellipsoid()
{
    set(WGS84_RADIUS_EQUATOR, WGS84_RADIUS_POLAR);
}

Ellipsoid::Ellipsoid(double er, double pr)
{
    set(er, pr);
}

glm::dmat4
Ellipsoid::topocentricToGeocentricMatrix(const glm::dvec3& geoc) const
{
    // start with a simple translation to the center point of the topocentric frame:
    auto l2w = glm::translate(glm::dmat4(1.0), geoc);

    // Apply a rotation to create a local tangent plane at thiat point:
    glm::dvec3 lla = geocentricToGeodetic(geoc);
    double latitude = deg2rad(lla.y);
    double longitude = deg2rad(lla.x);

    // calculate the E/N/U vectors:
    glm::dvec3 up(cos(longitude) * cos(latitude), sin(longitude) * cos(latitude), sin(latitude));
    glm::dvec3 east(-sin(longitude), cos(longitude), 0);
    glm::dvec3 north = glm::cross(up, east);

    // and apply them to the local-to-world matrix:
    l2w[0][0] = east[0];
    l2w[0][1] = east[1];
    l2w[0][2] = east[2];

    l2w[1][0] = north[0];
    l2w[1][1] = north[1];
    l2w[1][2] = north[2];

    l2w[2][0] = up[0];
    l2w[2][1] = up[1];
    l2w[2][2] = up[2];

    return l2w;
}

glm::dvec3
Ellipsoid::geodeticToGeocentric(const glm::dvec3& lla) const
{
    double latitude = deg2rad(lla.y);
    double longitude = deg2rad(lla.x);
 
    double sin_latitude = sin(latitude);
    double cos_latitude = cos(latitude);

    double N = _re / sqrt(1.0 - _ecc2 * sin_latitude * sin_latitude);

    return glm::dvec3(
        (N + lla.z) * cos_latitude * cos(longitude),
        (N + lla.z) * cos_latitude * sin(longitude),
        (N * (1.0 - _ecc2) + lla.z) * sin_latitude);
}

glm::dvec3
Ellipsoid::geocentricToGeodetic(const glm::dvec3& geoc) const
{
    double latitude = 0.0, longitude = 0.0, height = 0.0;

    // handle polar and center-of-earth cases directly.
    if (geoc.x != 0.0)
    {
        longitude = atan2(geoc.y, geoc.x);
    }
    else
    {
        if (geoc.y > 0.0)
        {
            longitude = PI_OVER_2;
        }
        else if (geoc.y < 0.0)
        {
            longitude = -PI_OVER_2;
        }
        else
        {
            // handle special cases (pole or at center of the earth)
            longitude = 0.0;
            if (geoc.z > 0.0)
            { // north pole.
                latitude = PI_OVER_2;
                height = geoc.z - _rp;
            }
            else if (geoc.z < 0.0)
            { // south pole.
                latitude = -PI_OVER_2;
                height = -geoc.z - _rp;
            }
            else
            { // center of earth.
                latitude = PI_OVER_2;
                height = -_rp;
            }

            return glm::dvec3(rad2deg(longitude), rad2deg(latitude), height);
        }
    }

    double p = sqrt(geoc.x * geoc.x + geoc.y * geoc.y);
    double theta = atan2(geoc.z * _re, (p * _rp));
    double eDashSquared = (_re * _re - _rp * _rp) / (_rp * _rp);

    double sin_theta = sin(theta);
    double cos_theta = cos(theta);

    latitude = atan(
        (geoc.z + eDashSquared * _rp * sin_theta * sin_theta * sin_theta) /
        (p - _ecc2 * _re * cos_theta * cos_theta * cos_theta));

    double sin_latitude = sin(latitude);
    double N = _re / sqrt(1.0 - _ecc2 * sin_latitude * sin_latitude);

    height = p / cos(latitude) - N;

    glm::dvec3 out(rad2deg(longitude), rad2deg(latitude), height);

    for (int i = 0; i < 3; ++i)
        if (std::isnan(out[i]))
            out[i] = 0.0;

    return out;
}

void
Ellipsoid::set(double re, double rp)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(re > 0.0 && rp > 0.0, void());
    _re = re;
    _rp = rp;

    double f = (_re - _rp) / _re;
    _ecc2 = (2.0 * f) - (f * f);

    _ellipsoidToUnitSphere = glm::dvec3(1.0 / re, 1.0 / re, 1.0 / rp);
    _unitSphereToEllipsoid = glm::dvec3(re, re, rp);
}

double
Ellipsoid::longitudinalDegreesToMeters(double value, double lat_deg) const
{
    return value * (2.0 * M_PI * semiMajorAxis() / 360.0) * cos(deg2rad(lat_deg));
}

double
Ellipsoid::metersToLongitudinalDegrees(double value, double lat_deg) const
{
    return value / longitudinalDegreesToMeters(1.0, lat_deg);
}

bool
Ellipsoid::intersectGeocentricLine(
    const glm::dvec3& p0_world,
    const glm::dvec3& p1_world,
    glm::dvec3& out_world) const
{
    double dist2 = 0.0;
    glm::dvec3 v;
    glm::dvec3 p0 = p0_world * _ellipsoidToUnitSphere;
    glm::dvec3 p1 = p1_world * _ellipsoidToUnitSphere;

    constexpr double R = 1.0; // for unit sphere.

    // http://paulbourke.net/geometry/circlesphere/index.html#linesphere

    glm::dvec3 d = p1 - p0;

    double A = glm::dot(d, d); //d * d;
    double B = 2.0 * glm::dot(d, p0); // (d * p0);
    double C = glm::dot(p0, p0) - R * R; // (p0 * p0) - R * R;

    // now solve the quadratic A + B*t + C*t^2 = 0.
    double D = B * B - 4.0*A*C;
    if (D > 0)
    {
        // two roots (line passes through sphere twice)
        // find the closer of the two.
        double sqrtD = sqrt(D);
        double t0 = (-B + sqrtD) / (2.0*A);
        double t1 = (-B - sqrtD) / (2.0*A);

        //seg; pick closest:
        if (fabs(t0) < fabs(t1))
            v = d * t0;
        else
            v = d * t1;
    }
    else if (D == 0.0)
    {
        // one root (line is tangent to sphere?)
        double t = -B / (2.0*A);
        v = d * t;
    }

    dist2 = glm::dot(v, v); // v.length2();

    if (dist2 > 0.0)
    {
        out_world = (p0 + v) * _unitSphereToEllipsoid;
        return true;
    }
    else
    {
        // either no intersection, or the distance was not the max.
        return false;
    }
}

double
Ellipsoid::geodesicGroundDistance(const glm::dvec3& p1, const glm::dvec3& p2) const
{
    double
        lat1 = deg2rad(p1.y),
        lon1 = deg2rad(p1.x),
        lat2 = deg2rad(p2.y),
        lon2 = deg2rad(p2.x);

    double Re = semiMajorAxis();
    double Rp = semiMinorAxis();
    double F = (Re - Rp) / Re; // flattening

    double B1 = atan((1.0 - F)*tan(lat1));
    double B2 = atan((1.0 - F)*tan(lat2));

    double P = (B1 + B2) / 2.0;
    double Q = (B2 - B1) / 2.0;

    double G = acos(sin(B1)*sin(B2) + cos(B1)*cos(B2)*cos(fabs(lon2 - lon1)));

    double
        sinG = sin(G),
        sinP = sin(P), sinQ = sin(Q),
        cosP = cos(P), cosQ = cos(Q),
        sinG2 = sin(G / 2.0), cosG2 = cos(G / 2.0);

    double X = (G - sinG)*((sinP*sinP*cosQ*cosQ) / (cosG2*cosG2));
    double Y = (G + sinG)*((cosP*cosP*sinQ*sinQ) / (sinG2*sinG2));

    double dist = Re * (G - (F / 2.0)*(X + Y));

    // NaN could mean start/end points are the same
    return std::isnan(dist) ? 0.0 : dist;
}

glm::dvec3
Ellipsoid::geodesicInterpolate(const glm::dvec3& lla1_deg, const glm::dvec3& lla2_deg, double t) const
{
    glm::dvec3 output;

    double deltaZ = lla2_deg.z - lla1_deg.z;

    // transform to unit-sphere frame:
    auto w1 = glm::normalize(geodeticToGeocentric(lla1_deg) * _ellipsoidToUnitSphere);
    auto w2 = glm::normalize(geodeticToGeocentric(lla2_deg) * _ellipsoidToUnitSphere);

    // Geometric slerp in unit sphere space
    // https://en.wikipedia.org/wiki/Slerp#Geometric_Slerp
    double dp = glm::dot(w1, w2);
    if (dp == 1.0)
    {
        return lla1_deg;
    }

    double angle = acos(dp);

    double s = sin(angle);
    if (s == 0.0)
    {
        return lla1_deg;
    }

    double c1 = sin((1.0 - t) * angle) / s;
    double c2 = sin(t * angle) / s;

    glm::dvec3 n = w1 * c1 + w2 * c2;

    // convert back to world space and apply altitude lerp
    n = n * _unitSphereToEllipsoid;

    output = geocentricToGeodetic(n);
    output.z = lla1_deg.z + t * deltaZ;

    return output;
}

glm::dvec3
Ellipsoid::calculateHorizonPoint(const std::vector<glm::dvec3>& points) const
{
    double max_magnitude = 0.0;

    glm::dvec3 unit_culling_point_dir; // vector along which to calculate the horizon point
    std::vector<glm::dvec3> unit_points;
    unit_points.reserve(points.size());
    for (auto& point : points)
    {
        unit_points.emplace_back(point * _ellipsoidToUnitSphere);
        unit_culling_point_dir += unit_points.back();
    }
    unit_culling_point_dir = glm::normalize(unit_culling_point_dir);

    for (auto& unit_point : unit_points)
    {
        auto mag2 = util::lengthSquared(unit_point);
        auto mag = sqrt(mag2);
        auto point_dir = unit_point / mag;

        // clamp to ellipsoid
        mag2 = std::max(1.0, mag2);
        mag = std::max(1.0, mag);

        auto cos_alpha = glm::dot(point_dir, unit_culling_point_dir);
        auto sin_alpha = glm::length(glm::cross(point_dir, unit_culling_point_dir));
        auto cos_beta = 1.0 / mag;
        auto sin_beta = sqrt(mag2 - 1.0) * cos_beta;

        auto culling_point_mag = 1.0 / (cos_alpha * cos_beta - sin_alpha * sin_beta);

        max_magnitude = std::max(max_magnitude, culling_point_mag);
    }

    auto unit_culling_point = unit_culling_point_dir * max_magnitude;
    return unit_culling_point * _unitSphereToEllipsoid;
}

glm::dvec3
Ellipsoid::rotationAxis(const glm::dvec3& geocStart, double initialCourse_deg) const
{
    const glm::dvec3 northPole(0, 0, 1);

    auto posUnit = glm::normalize(geocStart * _ellipsoidToUnitSphere);
    auto east = glm::normalize(glm::cross(northPole, posUnit));
    auto north = glm::normalize(glm::cross(posUnit, east));

    if (glm::length(east) < 1e-10)
    {
        east = glm::dvec3(1, 0, 0);
        north = glm::normalize(glm::cross(posUnit, east));
    }

    auto course_rad = deg2rad(initialCourse_deg);
    auto tangent = glm::normalize(north * cos(course_rad) + east * sin(course_rad));

    auto axisUnit = glm::normalize(glm::cross(posUnit, tangent));
    return glm::normalize(axisUnit * _unitSphereToEllipsoid);
}

double
Ellipsoid::course(const glm::dvec3& geocPoint, const glm::dvec3& geocAxis) const
{
    const glm::dvec3 northPole(0, 0, 1);
    auto posUnit = glm::normalize(geocPoint * _ellipsoidToUnitSphere);

    auto east = glm::normalize(glm::cross(northPole, posUnit));
    auto north = glm::normalize(glm::cross(posUnit, east));

    if (glm::length(east) < 1e-10) {
        east = glm::dvec3(1, 0, 0);
        north = glm::normalize(glm::cross(posUnit, east));
    }

    auto axisUnit = glm::normalize(geocAxis * _ellipsoidToUnitSphere);
    auto tangent = glm::normalize(glm::cross(axisUnit, posUnit));
    double n = glm::dot(tangent, north);
    double e = glm::dot(tangent, east);
    double heading_rad = atan2(e, n);
    return glm::degrees(heading_rad);
}


glm::dvec3
Ellipsoid::rotate(const glm::dvec3& geocPoint, const glm::dvec3& geocAxis, double angle_deg) const
{
    // convert to unit sphere:
    auto point_unit = geocPoint * _ellipsoidToUnitSphere;
    auto axis_unit = glm::normalize(geocAxis * _ellipsoidToUnitSphere);
    
    // rotate the point around the axis:
    glm::dquat rot = glm::angleAxis(util::deg2rad(angle_deg), axis_unit);
    auto output = rot * point_unit;
    return output * _unitSphereToEllipsoid;
}
