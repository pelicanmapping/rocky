/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>

namespace rocky
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

        //! Equatorial radius
        double getSemiMajorAxis() const;
        double getRadiusEquator() const { return getSemiMajorAxis(); }
        void setSemiMajorAxis(double value);

        //! Polar radius (meters)
        double getSemiMinorAxis() const;
        double getRadiusPolar() const { return getSemiMinorAxis(); }
        void setSemiMinorAxis(double value);

        //! Name of this ellipsoid (meters)
        const std::string& getName() const { return _name; }
        void setName(const std::string& value) { _name = value; }

        //! Matrix to transform from LTP at a point to geocentric
        dmat4 geocentricToLocalToWorld(const dvec3& xyz) const;

        //! Get local up vector at a geocentric point
        dvec3 geocentricToUpVector(const dvec3& xyz) const;

        //! Convert geocentric coords to geodetic (LL DEG + Alt M)
        dvec3 geocentricToGeodetic(const dvec3& xyz) const;

        //! Convert geodetic coords to geocentric coord
        dvec3 geodeticToGeocentric(const dvec3& lla) const;

        //! Get the coordinate frame at the geocentric point
        dmat4 geodeticToCoordFrame(const dvec3& xyz) const;

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

