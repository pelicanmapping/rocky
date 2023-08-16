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
        SRS srs;
        double x, y, z;

        //! Constructs an empty (and invalid) geopoint.
        GeoPoint();

        //! Constructs a GeoPoint at 0,0,0 absolute.
        GeoPoint(const SRS& srs);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, double x, double y);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, double x, double y, double z);

        //! Constructs a GeoPoint
        template<class T>
        GeoPoint(const SRS& srs, const T& dvec) :
            GeoPoint(srs, dvec.x, dvec.y, dvec.z) { }

        //! Destruct
        ~GeoPoint() { }

        //const SRS& srs() const { return _srs; }

        //! Gets copy of this geopoint transformed into another SRS
        bool transform(const SRS& outSRS, GeoPoint& output) const;

        template<class VEC3>
        inline bool transform(const SRS& outSRS, VEC3& output) const {
            GeoPoint temp;
            if (!transform(outSRS, temp)) return false;
            output = { temp.x, temp.y, temp.z };
            return true;
        }

        //! Transforms this point in place to another SRS
        bool transformInPlace(const SRS& srs);

        //! Geodesic distance from this point to another.
        //! This is the distance along the real-world ellipsoidal surface
        //! of the Earth (or other body), regardless of map projection.
        //! It does not account for Z/altitude.
        //! @param rhs Other point
        //! @return Geodesic distance between the two points
        Distance geodesicDistanceTo(const GeoPoint& rhs) const;


        bool operator == (const GeoPoint& rhs) const {
            return srs == rhs.srs && x == rhs.x && y == rhs.y && z == rhs.z;
        }

        bool operator != (const GeoPoint& rhs) const {
            return !operator==(rhs);
        }

        //! Does this object contain a valid geo point?
        bool valid() const {
            return srs.valid();
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
        //SRS _srs;
    };


    /**
    * Base class for any object that has a position on a Map.
    */
    class PositionedObject
    {
    public:
        //! Center position of the object
        virtual const GeoPoint& position() const = 0;
    };
}
