/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/SRS.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A georeferenced 3D point.
     */
    class ROCKY_EXPORT GeoPoint
    {
    public:
        //! Constructs an empty (and invalid) geopoint.
        GeoPoint();

        //! Constructs a GeoPoint at 0,0,0 absolute.
        GeoPoint(const SRS& srs);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, double x, double y);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, double x, double y, double z);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, const fvec3& xyz);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, const dvec3& xyz);

        //! Destruct
        ~GeoPoint() { }

        // component getter/setters

        double& x() { return _p.x; }
        double  x() const { return _p.x; }

        double& y() { return _p.y; }
        double  y() const { return _p.y; }

        double& z() { return _p.z; }
        double  z() const { return _p.z; }

        double& alt() { return _p.z; }
        double  alt() const { return _p.z; }

        double* ptr() { return &_p.x; }

        const dvec3& to_dvec3() const { return _p; }

        const SRS& srs() const { return _srs; }

        //! Gets copy of this geopoint transformed into another SRS
        bool transform(const SRS& outSRS, GeoPoint& output) const;

        //! Transforms this point in place to another SRS
        bool transformInPlace(const SRS& srs);

        //! Geodesic distance from this point to another.
        //! This is the distance along the real-world ellipsoidal surface
        //! of the Earth (or other body), regardless of map projection.
        //! It does not account for Z/altitude.
        //! @param rhs Other point
        //! @return Geodesic distance between the two points
        Distance geodesicDistanceTo(const GeoPoint& rhs) const;

        /**
         * @deprecated - ambiguous, will be removed. Use geodesicDistanceTo() or toWorld()/length instead.
         * Calculates the distance in meters from this geopoint to another.
         */
        double distanceTo(const GeoPoint& rhs) const;

        /**
         * Interpolates a point between this point and another point
         * using the parameter t [0..1].
         */
         //GeoPoint interpolate(const GeoPoint& rhs, double t) const;

         //! Convenience function to return xy units
         //Units getXYUnits() const;

        bool operator == (const GeoPoint& rhs) const {
            return _srs == rhs._srs && _p == rhs._p;
        }

        bool operator != (const GeoPoint& rhs) const {
            return !operator==(rhs);
        }

        //! Does this object contain a valid geo point?
        bool valid() const {
            return _srs.valid();
        }

    public:
        static GeoPoint INVALID;

        // copy/move ops
        GeoPoint(const GeoPoint& rhs) = default;
        GeoPoint& operator=(const GeoPoint& rhs) = default;
        GeoPoint(GeoPoint&& rhs) { *this = rhs; }
        GeoPoint& operator=(GeoPoint&& rhs);

        //JSON to_json() const;

    private:
        dvec3 _p;
        SRS _srs;
    };
}
