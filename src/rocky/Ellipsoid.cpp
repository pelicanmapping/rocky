/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Ellipsoid.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;

// https://en.wikipedia.org/wiki/World_Geodetic_System
#define WGS84_RADIUS_EQUATOR 6378137.0
#define WGS84_RADIUS_POLAR 6356752.314245

#define PI_OVER_2 (0.5*M_PI)

namespace
{
    inline void computeCoordinateFrame(double latitude, double longitude, glm::dmat4& localToWorld)
    {
        // Compute up vector
        glm::dvec3 up(cos(longitude)*cos(latitude), sin(longitude)*cos(latitude), sin(latitude));

        // Compute east vector
        glm::dvec3 east(-sin(longitude), cos(longitude), 0);

        // Compute north vector = outer product up x east
        glm::dvec3 north = glm::cross(up, east);

        localToWorld[0][0] = east[0];
        localToWorld[0][1] = east[1];
        localToWorld[0][2] = east[2];

        localToWorld[1][0] = north[0];
        localToWorld[1][1] = north[1];
        localToWorld[1][2] = north[2];

        localToWorld[2][0] = up[0];
        localToWorld[2][1] = up[1];
        localToWorld[2][2] = up[2];
    }

    inline void convertLatLongHeightToXYZ(
        double RE, double RP, double ECC2,
        double latitude, double longitude, double height,
        double& X, double& Y, double& Z)
    {
        // for details on maths see http://www.colorado.edu/geography/gcraft/notes/datum/gif/llhxyz.gif
        double sin_latitude = sin(latitude);
        double cos_latitude = cos(latitude);
        double N = RE / sqrt(1.0 - ECC2 * sin_latitude*sin_latitude);
        X = (N + height)*cos_latitude*cos(longitude);
        Y = (N + height)*cos_latitude*sin(longitude);
        Z = (N*(1 - ECC2) + height)*sin_latitude;
    }


    inline void convertXYZToLatLongHeight(
        double RE, double RP, double ECC2,
        double X, double Y, double Z,
        double& latitude, double& longitude, double& height)
    {
        // handle polar and center-of-earth cases directly.
        if (X != 0.0)
            longitude = atan2(Y, X);
        else
        {
            if (Y > 0.0)
                longitude = PI_OVER_2;
            else if (Y < 0.0)
                longitude = -PI_OVER_2;
            else
            {
                // at pole or at center of the earth
                longitude = 0.0;
                if (Z > 0.0)
                { // north pole.
                    latitude = PI_OVER_2;
                    height = Z - RP;
                }
                else if (Z < 0.0)
                { // south pole.
                    latitude = -PI_OVER_2;
                    height = -Z - RP;
                }
                else
                { // center of earth.
                    latitude = PI_OVER_2;
                    height = -RP;
                }
                return;
            }
        }

        // http://www.colorado.edu/geography/gcraft/notes/datum/gif/xyzllh.gif
        double p = sqrt(X*X + Y * Y);
        double theta = atan2(Z*RE, (p*RP));
        double eDashSquared = (RE*RE - RP * RP) /
            (RP*RP);

        double sin_theta = sin(theta);
        double cos_theta = cos(theta);

        latitude = atan((Z + eDashSquared * RP*sin_theta*sin_theta*sin_theta) /
            (p - ECC2 * RE*cos_theta*cos_theta*cos_theta));

        double sin_latitude = sin(latitude);
        double N = RE / sqrt(1.0 - ECC2 * sin_latitude*sin_latitude);

        height = p / cos(latitude) - N;
    }

    inline void computeLocalToWorldTransformFromLatLongHeight(
        double RE, double RP, double ECC2,
        double latitude,
        double longitude,
        double height,
        glm::dmat4& localToWorld)
    {
        double X, Y, Z;
        convertLatLongHeightToXYZ(RE, RP, ECC2, latitude, longitude, height, X, Y, Z);

        localToWorld = glm::translate(glm::dmat4(1.0), glm::dvec3(X, Y, Z));
        //localToWorld.makeTranslate(X, Y, Z);
        computeCoordinateFrame(latitude, longitude, localToWorld);
    }

    inline void computeLocalToWorldTransformFromXYZ(
        double RE, double RP, double ECC2,
        double X, double Y, double Z,
        glm::dmat4& localToWorld)
    {
        double  latitude, longitude, height;
        convertXYZToLatLongHeight(RE, RP, ECC2, X, Y, Z, latitude, longitude, height);

        localToWorld = glm::translate(glm::dmat4(1.0), glm::dvec3(X, Y, Z));
        //localToWorld.makeTranslate(X, Y, Z);
        computeCoordinateFrame(latitude, longitude, localToWorld);
    }

    inline glm::dvec3 computeLocalUpVector(
        double RE, double RP, double ECC2,
        double X, double Y, double Z)
    {
        // Note latitude is angle between normal to ellipsoid surface and XY-plane
        double  latitude;
        double  longitude;
        double  altitude;
        convertXYZToLatLongHeight(RE, RP, ECC2, X, Y, Z, latitude, longitude, altitude);

        // Compute up vector
        return glm::dvec3(
            cos(longitude) * cos(latitude),
            sin(longitude) * cos(latitude),
            sin(latitude));
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

Ellipsoid::~Ellipsoid()
{
    //nop
}

void
Ellipsoid::setSemiMajorAxis(double value)
{
    set(value, _rp);
}

double
Ellipsoid::semiMajorAxis() const
{
    return _re;
}

void
Ellipsoid::setSemiMinorAxis(double value)
{
    set(_re, value);
}

double
Ellipsoid::semiMinorAxis() const
{
    return _rp;
}

glm::dmat4
Ellipsoid::geocentricToLocalToWorld(const glm::dvec3& geoc) const
{
    glm::dmat4 local2world;
    double latitude, longitude, height;
    convertXYZToLatLongHeight(_re, _rp, _ecc2, geoc.x, geoc.y, geoc.z, latitude, longitude, height);

    local2world = glm::translate(glm::dmat4(1.0), geoc);
    //localToWorld.makeTranslate(geoc.x, geoc.y, geoc.z);
    computeCoordinateFrame(latitude, longitude, local2world);
    return local2world;
}

glm::dvec3
Ellipsoid::geocentricToUpVector(const glm::dvec3& geoc) const
{
    return computeLocalUpVector(
        _re, _rp, _ecc2, geoc.x, geoc.y, geoc.z);
}

glm::dmat4
Ellipsoid::geodeticToCoordFrame(const glm::dvec3& lla) const
{
    glm::dmat4 m(1);
    computeCoordinateFrame(deg2rad(lla.y), deg2rad(lla.x), m);
    return m;
}

glm::dvec3
Ellipsoid::geodeticToGeocentric(const glm::dvec3& lla) const
{
    glm::dvec3 out;
    convertLatLongHeightToXYZ(
        _re, _rp, _ecc2,
        deg2rad(lla.y), deg2rad(lla.x), lla.z,
        out.x, out.y, out.z);
    return out;
}

glm::dvec3
Ellipsoid::geocentricToGeodetic(const glm::dvec3& xyz) const
{
    double lat_rad, lon_rad, height_m;
    convertXYZToLatLongHeight(
        _re, _rp, _ecc2,
        xyz.x, xyz.y, xyz.z,
        lat_rad, lon_rad, height_m);

    glm::dvec3 out(rad2deg(lon_rad), rad2deg(lat_rad), height_m);

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

    _ellipsoidToUnitSphere = glm::dmat3(
        1.0 / re, 0, 0,
        0, 1.0 / re, 0,
        0, 0, 1.0 / rp);

    _unitSphereToEllipsoid = glm::dmat3(
        re, 0, 0,
        0, re, 0,
        0, 0, rp);

    //_ellipsoidToUnitSphere.makeScale(1.0 / er, 1.0 / er, 1.0 / pr);
    //_unitSphereToEllipsoid.makeScale(er, er, pr);
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
Ellipsoid::geodesicGroundDistance(
    const glm::dvec3& p1,
    const glm::dvec3& p2) const
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

void
Ellipsoid::geodesicInterpolate(
    const glm::dvec3& lla1_deg,
    const glm::dvec3& lla2_deg,
    double t,
    glm::dvec3& output) const
{
    double deltaZ = lla2_deg.z - lla1_deg.z;

    glm::dvec3 w1 = geodeticToGeocentric(lla1_deg) * _ellipsoidToUnitSphere;
    w1 = glm::normalize(w1);
    //w1.normalize();

    glm::dvec3 w2 = geodeticToGeocentric(lla2_deg) * _ellipsoidToUnitSphere;
    w2 = glm::normalize(w2);
    //w2.normalize();

    // Geometric slerp in unit sphere space
    // https://en.wikipedia.org/wiki/Slerp#Geometric_Slerp
    double dp = glm::dot(w1, w2); // w1 * w2;
    if (dp == 1.0)
    {
        output = lla1_deg;
        return;
    }

    double angle = acos(dp);

    double s = sin(angle);
    if (s == 0.0)
    {
        output = lla1_deg;
        return;
    }

    double c1 = sin((1.0 - t)*angle) / s;
    double c2 = sin(t*angle) / s;

    glm::dvec3 n = w1 * c1 + w2 * c2;

    // convert back to world space and apply altitude lerp
    n = n * _unitSphereToEllipsoid;

    output = geocentricToGeodetic(n);
    output.z = lla1_deg.z + t * deltaZ;
}
