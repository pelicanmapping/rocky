/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A 2-axis ellipsoid used to approximate the shape of 
     * the Earth or other planetary body.
     */
    class ROCKY_EXPORT Ellipsoid
    {
    public:
        //! Construct a default WGS84 ellipsoid
        Ellipsoid();

        //! Construct an ellipsoid
        //! @param semiMajorRadius Radius at the equator (meters)
        //! @param semiMinorRadius Radius Radius of the poles (meters)
        Ellipsoid(
            double semiMajorRadius,
            double semiMinorRadius);

        //! Equatorial radius (meters)
        double semiMajorAxis() const;

        //! Equatorial radius (meters)
        void setSemiMajorAxis(double value);

        //! Polar radius (meters)
        double semiMinorAxis() const;

        //! Polar radius (meters)
        void setSemiMinorAxis(double value);

        //! Name of this ellipsoid
        const std::string& name() const { return _name; }

        //! Name of this ellipsoid
        void setName(const std::string& value) { _name = value; }

        //! Matrix to transform from LTP at a point to geocentric
        //! @param xyz Input geocentric point
        //! @return Matrix that transforms points from geocentric space to the tangent place XYZ space
        dmat4 geocentricToLocalToWorld(const dvec3& geocPoint) const;

        //! Get local up vector at a geocentric point
        dvec3 geocentricToUpVector(const dvec3& geocPoint) const;

        //! Convert geocentric coords to geodetic
        //! @param geocPoint Input geocentric point (x, y, z meters)
        //! @return output geodetic (degrees longitude, degrees latitude, meters altitude) point
        dvec3 geocentricToGeodetic(const dvec3& geocPoint) const;

        //! Convert geodetic coords to geocentric coord
        dvec3 geodeticToGeocentric(const dvec3& geodPoint) const;

        //! Get the coordinate frame at the geocentric point
        dmat4 geodeticToCoordFrame(const dvec3& geodPoint) const;

        //! Converts degrees to meters at a given latitide
        double longitudinalDegreesToMeters(double value, double lat_deg = 0.0) const;

        //! Converts meters to degrees at a given latitide
        double metersToLongitudinalDegrees(double value, double lat_deg =0.0) const;

        //! Geodesic distance in meters from one lat/long to another
        double geodesicDistance(
            const dvec2& longlat1_deg, 
            const dvec2& longlat2_deg) const;

        //! Geodesic interpolation between two lat/long points
        void geodesicInterpolate(
            const dvec3& longlat1_deg,
            const dvec3& longlat2_deg,
            double t,
            dvec3& out_longlat_deg) const;

        //! Intersects a geocentric line with the ellipsoid.
        //! Upon success return true and place the first intersection
        //! point in "out".
        bool intersectGeocentricLine(
            const dvec3& p0,
            const dvec3& p1,
            dvec3& out) const;

        //! dtor
        ~Ellipsoid();

    private:
        void set(double er, double pr);

        double _re, _rp, _ecc2;
        std::string _name;
        dmat3 _ellipsoidToUnitSphere;
        dmat3 _unitSphereToEllipsoid;
    };
}

