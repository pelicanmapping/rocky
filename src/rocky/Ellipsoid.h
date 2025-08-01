/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>
#include <vector>

namespace ROCKY_NAMESPACE
{
    /**
     * A 2-axis ellipsoid used to approximate the shape of 
     * the Earth or other planetary body.
     */
    class ROCKY_EXPORT Ellipsoid
    {
    public:
        //! Construct a WGS84 ellipsoid
        Ellipsoid();

        //! Construct an ellipsoid
        //! @param semiMajorRadius Radius at the equator (meters)
        //! @param semiMinorRadius Radius Radius of the poles (meters)
        Ellipsoid(double semiMajorRadius, double semiMinorRadius);

        //! Equatorial radius (meters)
        double semiMajorAxis() const;

        //! Polar radius (meters)
        double semiMinorAxis() const;

        //! Name of this ellipsoid
        const std::string& name() const { return _name; }

        //! Name of this ellipsoid
        void setName(const std::string& value) { _name = value; }

        //! Matrix to transform from a topocentric (local tangent plane) frame centered at
        //! the provided geocentric point to geocentric coordinates.
        //! @param xyz Geocentric origin point of the topocentric frame
        //! @return topocentric to geocentric matrix
        glm::dmat4 topocentricToGeocentricMatrix(const glm::dvec3& geocPoint) const;

        //! Convert geocentric coords to geodetic
        //! @param geocPoint Input geocentric point (x, y, z meters)
        //! @return output geodetic (degrees longitude, degrees latitude, meters altitude) point
        glm::dvec3 geocentricToGeodetic(const glm::dvec3& geocPoint) const;

        //! Convert geodetic coords to geocentric coord
        //! @param geodPoint Input geodetic point (degrees longitude, degrees latitude, meters altitude)
        //! @return output geocentric (x, y, z meters) point
        glm::dvec3 geodeticToGeocentric(const glm::dvec3& geodPoint) const;

        //! Converts degrees to meters at a given latitide
        //! @param value Degrees to convert
        //! @param lat_deg Reference latitude in degrees
        //! @return Meters
        double longitudinalDegreesToMeters(double value, double lat_deg = 0.0) const;

        //! Converts meters to degrees at a given latitide
        //! @param value Meters to convert
        //! @param lat_deg Reference latitude in degrees
        //! @return Degrees
        double metersToLongitudinalDegrees(double value, double lat_deg =0.0) const;

        //! Geodesic distance in meters from one lat/long to another
        double geodesicGroundDistance(
            const glm::dvec3& longlat1_deg,
            const glm::dvec3& longlat2_deg) const;

        //! Geodesic interpolation between two lat/long points
        //! @param longlat1_deg Start point in degrees (altitude in meters)
        //! @param longlat2_deg End point in degrees (altitude in meters)
        //! @param t Interpolation factor (0 = start, 1 = end)
        //! @return Interpolated point in degrees (altitude in meters)
        glm::dvec3 geodesicInterpolate(
            const glm::dvec3& longlat1_deg,
            const glm::dvec3& longlat2_deg,
            double t) const;

        //! Intersects a geocentric line with the ellipsoid.
        //! Upon success return true and place the first intersection point in "out".
        //! @param geocStart Start point of the geocentric line
        //! @param geocEnd End point of the geocentric line
        //! @param geocOut Output geocentric intersection point
        bool intersectGeocentricLine(
            const glm::dvec3& geocStart,
            const glm::dvec3& geocEnd,
            glm::dvec3& geocOut) const;

        //! Rotates the geocentric point along the a great ellipse.
        //! @param geocPoint Geocentric point to rotate
        //! @param geocAxis Axis of rotation (geocentric)
        //! @param angle_deg Angle to rotate in degrees
        //! @return Rotated geocentric point
        glm::dvec3 rotate(const glm::dvec3& geocPoint, const glm::dvec3& geocAxis, double angle_deg) const;

        //! Calculates a geocentric point that can be used for horizon-culling;
        //! i.e. if the horizon point is visible over the horizon, that means that
        //! at least one point in the input vector are visible as well.
        //! @param points Vector of geocentric points
        //! @return Geocentric point that can be used for horizon culling
        glm::dvec3 calculateHorizonPoint(const std::vector<glm::dvec3>& points) const;

        //! Calculates the rotation axis that will rotate a point along an ellipsoidal
        //! path at a provided initial bearing.
        //! @param geocPoint Geocentric start point (x, y, z in meters)
        //! @param initialBearing_deg Initial bearing in degrees relative to the local tangent place at geocPoint
        //! @return Rotation axis in geocentric coordinates (x, y, z in meters)
        glm::dvec3 rotationAxis(const glm::dvec3& geocPoint, double initialBearing_deg) const;

        //! Equality operator
        inline bool operator == (const Ellipsoid& rhs) const
        {
            return _re == rhs._re && _rp == rhs._rp;
        }

        //! Inequality operator
        inline bool operator != (const Ellipsoid& rhs) const
        {
            return _re != rhs._re || _rp != rhs._rp;
        }

    private:
        void set(double er, double pr);

        double _re, _rp, _ecc2;
        std::string _name;
        glm::dvec3 _ellipsoidToUnitSphere;
        glm::dvec3 _unitSphereToEllipsoid;
    };
}

