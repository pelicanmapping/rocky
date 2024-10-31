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

        //! Cast to a vector.
        explicit operator glm::dvec3 () const { return { x, y, z }; }

        //! Transforms this geopoint into another SRS.
        //! @param outSRS The target SRS
        //! @return The transformed geopoint
        GeoPoint transform(const SRS& outSRS) const;

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

        //! Return a readable string
        std::string string() const;

    public:
        static GeoPoint INVALID;

        // copy/move ops
        GeoPoint(const GeoPoint& rhs) = default;
        GeoPoint& operator=(const GeoPoint& rhs) = default;
        GeoPoint(GeoPoint&& rhs) noexcept = default;
        GeoPoint& operator=(GeoPoint&& rhs) noexcept = default;
    };


    /**
    * Base class for any object that has a position on a Map.
    */
    class PositionedObject
    {
    public:
        //! Center position of the object
        virtual const GeoPoint& objectPosition() const = 0;
    };
}
