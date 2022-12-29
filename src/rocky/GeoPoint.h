/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoCommon.h>
#include <rocky/SRS.h>

namespace rocky
{
    class TerrainResolver;

    /**
     * A georeferenced 3D point.
     */
    class ROCKY_EXPORT GeoPoint
    {
    public:

        //! Constructs an empty (and invalid) geopoint.
        GeoPoint();

        //! Copy
        GeoPoint(const GeoPoint& rhs);

        //! Constructs a GeoPoint at 0,0,0 absolute.
        GeoPoint(shared_ptr<SRS> srs);

        //! Constructs a GeoPoint
        GeoPoint(
            shared_ptr<SRS> srs,
            double x,
            double y,
            double z,
            const AltitudeMode& mode =ALTMODE_ABSOLUTE);

        //! Constructs a GeoPoint with X and Y coordinates. The Z defaults
        //! to zero with an ALTMODE_RELATIVE altitude mode (i.e., 0 meters)
        GeoPoint(
            shared_ptr<SRS> srs,
            double x,
            double y);

        GeoPoint(
            shared_ptr<SRS> srs,
            const dvec3& xyz,
            const AltitudeMode& altmode = ALTMODE_ABSOLUTE);

        //! Constructs a new GeoPoint by transforming an existing GeoPoint into
        //! the specified spatial reference.
        GeoPoint(
            shared_ptr<SRS> srs,
            const GeoPoint& rhs);

        //! Constructs a geopoint from serialization
        GeoPoint(
            const Config& conf,
            shared_ptr<SRS> srs = nullptr);


        /** dtor */
        virtual ~GeoPoint() { }

        //! Sets the SRS and coords
        void set(
            shared_ptr<SRS> srs,
            const dvec3& xyz,
            const AltitudeMode& mode);

        //! Sets the SRS and coords
        void set(
            shared_ptr<SRS> srs,
            double                  x,
            double                  y,
            double                  z,
            const AltitudeMode&     mode);

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

        shared_ptr<SRS> getSRS() const { return _srs; }

        /**
         * AltitudeMode reflects whether the Z coordinate is absolute with
         * respect to MSL or relative to the terrain elevation at that
         * point. When using relative points, GeoPoint usually requires
         * access to a TerrainProvider in order to resolve the altitude.
         */
        AltitudeMode& altitudeMode() { return _altMode; }
        const AltitudeMode& altitudeMode() const { return _altMode; }
        bool isRelative() const { return _altMode == ALTMODE_RELATIVE; }
        bool isAbsolute() const { return _altMode == ALTMODE_ABSOLUTE; }

        /**
         * Returns a copy of this geopoint transformed into another SRS.
         */
        GeoPoint transform(shared_ptr<SRS> outSRS) const;

        /**
         * Transforms this geopoint into another SRS.
         */
        bool transform(shared_ptr<SRS> outSRS, GeoPoint& output) const;

        /**
         * Transforms this point in place to another SRS.
         */
        bool transformInPlace(shared_ptr<SRS> srs);

        /**
         * Transforms this geopoint's Z coordinate (in place)
         */
        bool transformZ(const AltitudeMode& altMode, const TerrainResolver* t);

        /**
         * Transforms and returns the geopoints Z coordinate.
         */
        bool transformZ(const AltitudeMode& altMode, const TerrainResolver* t, double& out_z) const;

        //! Transforms a resolution distance to a cartesian value.
        Distance transformResolution(const Distance& d, const Units& outUnits) const;

        /**
         * Transforms this geopoint's Z to be absolute w.r.t. the vertical datum
         */
        bool makeAbsolute(const TerrainResolver* t) { return transformZ(ALTMODE_ABSOLUTE, t); }

        /**
         * Transforms this geopoint's Z to be terrain-relative.
         */
        bool makeRelative(const TerrainResolver* t) { return transformZ(ALTMODE_RELATIVE, t); }

        /**
         * Transforms this GeoPoint to geographic (lat/long) coords in place.
         */
        bool makeGeographic();

        /**
         * Outputs world coordinates corresponding to this point. If the point
         * is ALTMODE_RELATIVE, this will fail because there's no way to resolve
         * the actual Z coordinate. Use the variant of toWorld that takes a
         * Terrain* instead.
         */
        template<class DVEC3>
        inline bool toWorld(DVEC3& out_world) const;

        /**
         * Outputs world coordinates corresponding to this point, passing in a Terrain
         * object that will be used if the point needs to be converted to absolute
         * altitude
         */
        bool toWorld(dvec3& out_world, const TerrainResolver* terrain) const;

        /**
         * Converts world coordinates into a geopoint
         */
        template<class DVEC3>
        inline bool fromWorld(shared_ptr<SRS> srs, const DVEC3& world);

        /**
         * geopoint into absolute world coords.
         */
        bool createLocalToWorld(dmat4& out_local2world) const;

        /**
         * Outputs a matrix that will transform absolute world coordiantes so they are
         * localized into a local tangent place around this geopoint.
         */
        bool createWorldToLocal(dmat4& out_world2local) const;

        /**
         * Converts this point to the same point in a local tangent plane.
         */
        GeoPoint toLocalTangentPlane() const;

        /**
         * Outputs an "up" vector corresponding to the given point. The up vector
         * is orthogonal to a local tangent plane at that point on the map.
         */
        bool createWorldUpVector(dvec3& out_up) const;

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
        GeoPoint interpolate(const GeoPoint& rhs, double t) const;

        //! Convenience function to return xy units
        const Units& getXYUnits() const;

        bool operator == (const GeoPoint& rhs) const;
        bool operator != (const GeoPoint& rhs) const { return !operator==(rhs); }
        bool valid() const { return _srs != nullptr; }

        Config getConfig() const;

        /**
         * Represent this point as a string
         */
        std::string toString() const;

    public:
        static GeoPoint INVALID;

    protected:
        dvec3 _p;
        shared_ptr<SRS> _srs;
        AltitudeMode _altMode;

        bool toWorld_impl(dvec3& out) const;
        bool fromWorld_impl(shared_ptr<SRS> srs, const dvec3& in);
    };



    template<class DVEC3>
    bool GeoPoint::toWorld(DVEC3& out_dvec3) const
    {
        dvec3 temp;
        bool ok = toWorld_impl(temp);
        out_dvec3 = DVEC3(temp.x, temp.y, temp.z);
        return ok;
    }

    template<class DVEC3>
    bool GeoPoint::fromWorld(shared_ptr<SRS> srs, const DVEC3& in)
    {
        return fromWorld_impl(srs, dvec3(in.x, in.y, in.z));
    }
}
