/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Units.h>
#include <rocky/Ellipsoid.h>

namespace ROCKY_NAMESPACE
{
    class SRSTransform;

    /**
    * Spatial reference system.
    */
    class ROCKY_EXPORT SRS
    {
    public:
        // Commonly used SRS's.

        //! Latitude and Longitude on the WGS84 ellipsoid
        //! https://en.wikipedia.org/wiki/World_Geodetic_System
        static const SRS WGS84;

        //! Earth Centered Earth Fixed (Geocentric cartesian)
        //! https://en.wikipedia.org/wiki/Earth-centered,_Earth-fixed_coordinate_system
        static const SRS ECEF;

        //! Spherical Mercator, most common SRS for web maps
        //! https://proj.org/operations/projections/merc.html
        static const SRS SPHERICAL_MERCATOR;

        //! Equidistant Cylindrical
        //! https://proj.org/operations/projections/eqc.html
        static const SRS PLATE_CARREE;

        //! Empty invalid SRS
        static const SRS EMPTY;

    public:
        //! Construct an empty (invalid) SRS.
        SRS();

        //! Construct a new SRS from a definition string and
        //! an optional vertical datum string.
        SRS(
            const std::string& horizontal,
            const std::string& vertical = {});

        //! Make a transform from this SRS to another
        SRSTransform to(const SRS&) const;

        //! Name of this SRS
        const char* name() const;

        //! Definition used to initialize this SRS
        const std::string& definition() const {
            return _definition;
        }

        //! Vertical datum definition, if it exists
        const std::string& vertical() const {
            return _vertical;
        }

        //! Whether this is a valid SRS
        bool valid() const {
            return _valid;
        }

        //! Is this a geographic (long, lat) SRS?
        bool isGeographic() const;

        //! Is this projected (XY) SRS?
        bool isProjected() const;

        //! Is this a geocentric (ECEF) SRS?
        bool isGeocentric() const;

        //! Gets the underlying geographic/geodetic SRS
        SRS geoSRS() const;

        //! WKT (OGC Well-Known Text) representation
        const std::string& wkt() const;

        //! Units of measure
        const Units& units() const;

        //! Underlying reference ellipsoid
        const Ellipsoid& ellipsoid() const;

        //! Bounding box, if known
        const Box& bounds() const;

        //! Whether this SRS is mathematically equivalent to another SRS
        //! without taking vertical datums into account.
        bool isHorizEquivalentTo(const SRS& rhs) const;

        //! Whether this SRS is mathematically equivalent to another SRS
        bool isEquivalentTo(const SRS& rhs) const;

        //! Equality is the same as equivalency
        bool operator == (const SRS& rhs) const {
            return isEquivalentTo(rhs);
        }

        //! Equality is the same as equivalency
        bool operator != (const SRS& rhs) const {
            return !operator==(rhs);
        }

        //! Make a matrix that will transform coordinates from a topocentric
        //! ENU coordinate system (e.g., a local tangent plane) centered at
        //! the provided origin into cartesian world coordinates (geocentric if
        //! the SRS is geographic; projected if the SRS is projected).
        dmat4 localToWorldMatrix(const dvec3& origin) const;

        //! Units transformation accounting for latitude if necessary
        static double transformUnits(
            double input,
            const SRS& fromSRS,
            const SRS& toSRS,
            const Angle& latitude);

        //! Units transformation accounting for latitude if necessary
        static double transformUnits(
            const Distance& distance,
            const SRS& outSRS,
            const Angle& latitude);

        // copy/move operations
        SRS(const SRS& rhs) = default;
        SRS& operator=(const SRS& rhs) = default;
        SRS(SRS&& rhs) { *this = rhs; }
        SRS& SRS::operator=(SRS&&);
        ~SRS();

    private:
        //! Create an SRS from an initialization string.
        std::string _definition;
        std::string _vertical;
        bool _valid;
        friend class SRSTransform;
    };

    //! Convenient symonym
    using SpatialReference = SRS;


    class ROCKY_EXPORT SRSTransform
    {
    public:
        //! Construct an empty (invalid) transform
        SRSTransform();

        //! Whether this is a valid and legal transform
        bool valid() const;

        //! Source SRS of the transformation
        const SRS& from() const {
            return _from;
        }

        //! Target SRS of the transformation
        const SRS& to() const {
            return _to;
        }

        template<typename DVEC3A, typename DVEC3B>
        bool transform(const DVEC3A& in, DVEC3B& out) const {
            out.x = in.x, out.y = in.y, out.z = in.z;
            return forward(get_handle(), out.x, out.y, out.z);
        }

        template<typename DVEC3A, typename DVEC3B>
        bool operator()(const DVEC3A& in, DVEC3B& out) const {
            out.x = in.x, out.y = in.y, out.z = in.z;
            return forward(get_handle(), out.x, out.y, out.z);
        }

        template<typename ITERATOR>
        bool transformRange(ITERATOR begin, ITERATOR end) const {
            unsigned errors = 0;
            void* handle = get_handle();
            for (auto iter = begin; iter != end; ++iter)
                if (!forward(handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        template<typename DVEC3A, typename DVEC3B>
        bool inverse(const DVEC3A& in, DVEC3B& out) const {
            out = in;
            return inverse(get_handle(), out.x, out.y, out.z);
        }

        template<typename ITERATOR>
        void inverseRange(ITERATOR begin, ITERATOR end) const {
            unsigned errors = 0;
            void* handle = get_handle();
            for (auto iter = begin; iter != end; ++iter)
                if (!inverse(handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        // copy/move ops
        SRSTransform(const SRSTransform& rhs) = default;
        SRSTransform& operator=(const SRSTransform&) = default;
        SRSTransform(SRSTransform && rhs) { *this = rhs; }
        SRSTransform& operator=(SRSTransform&&);
        ~SRSTransform();

    private:
        SRS _from;
        SRS _to;

        void* get_handle() const;
        bool forward(void* handle, double& x, double& y, double& z) const;
        bool inverse(void* handle, double& x, double& y, double& z) const;

        friend class SRS;
    };
}
