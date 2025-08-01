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
    class ROCKY_EXPORT GeoPoint : public glm::dvec3
    {
    public:
        SRS srs;

        //! Constructs an empty (and invalid) geopoint.
        GeoPoint();

        //! Constructs a GeoPoint at 0,0,0 absolute.
        GeoPoint(const SRS& srs);

        //! Constructs a GeoPoint
        GeoPoint(const SRS& srs, double x, double y, double z = 0.0);

        //! Constructs a GeoPoint
        template<class T>
        GeoPoint(const SRS& srs, const T& dvec) :
            GeoPoint(srs, dvec.x, dvec.y, dvec.z) { }

        //! Assign a vec3
        GeoPoint& operator = (const glm::dvec3& rhs) {
            x = rhs.x, y = rhs.y, z = rhs.z;
            return *this;
        }

        //! Transforms this geopoint into another SRS.
        //! @param outSRS The target SRS
        //! @return The transformed geopoint
        GeoPoint transform(const SRS& outSRS) const;

        //! Transforms this point in place to another SRS
        //! @param srs The target SRS
        //! @return This GeoPoint
        GeoPoint& transformInPlace(const SRS& srs);

        //! Geodesic distance from this point to another.
        //! This is the distance along the real-world ellipsoidal surface
        //! of the planet, regardless of map projection.
        //! It does not account for Z/altitude.
        //! @param rhs Other point
        //! @return Geodesic distance between the two points
        Distance geodesicDistanceTo(const GeoPoint& rhs) const;

        //! Checks for equality
        bool operator == (const GeoPoint& rhs) const {
            return srs == rhs.srs && x == rhs.x && y == rhs.y && z == rhs.z;
        }

        //! Checks for inequality
        bool operator != (const GeoPoint& rhs) const {
            return !operator==(rhs);
        }

        //! Does this object represent a valid geo point?
        inline bool valid() const {
            return srs.valid();
        }

        //! Readable string
        std::string string() const;

    public:
        static GeoPoint INVALID;

        // copy/move ops
        GeoPoint(const GeoPoint& rhs) = default;
        GeoPoint& operator=(const GeoPoint& rhs) = default;
        GeoPoint(GeoPoint&& rhs) noexcept = default;
        GeoPoint& operator=(GeoPoint&& rhs) noexcept = default;
    };
}
