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
    class SRSOperation;

    /**
    * Spatial reference system.
    * An SRS is the context that makes a coordinates geospatially meaningful.
    */
    class ROCKY_EXPORT SRS
    {
    public:
        // Commonly used SRS's.
        static const SRS WGS84;
        static const SRS ECEF;
        static const SRS SPHERICAL_MERCATOR;
        static const SRS PLATE_CARREE;
        static const SRS EMPTY;

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

        //! Construct a new SRS from defintion strings.
        //! 
        //! @param horizontal Main definition string may be WKT definition, 
        //!     a PROJ init string, or an epsg code (like epsg:4326), or a
        //!     well-known alias like "wgs84" or "spherical-mercator".
        //! @param vertical Vertical datum string, typically the local filename
        //!     of a grid file (like "us_nga_egm96_15m.tif") found in the 
        //!     PROJ_DATA folder
        SRS(
            const std::string& horizontal,
            const std::string& vertical = {});

        //! Make an operation that will take coordinates from this SRS to another
        //! @param target Target SRS for coordinate operation
        SRSOperation to(const SRS& target) const;

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

        //! Units of measure for the horizontal components
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
        //! (Note: in this is a geographic SRS, the LTP will
        //! be in geocentric cartesian space.)
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

        //! If the event of an error, return the last error message
        const std::string& errorMessage() const;

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
        friend class SRSOperation;
    };

    //! Convenient symonym
    using SpatialReference = SRS;


    /**
    * Coordinate operation that translates coordinates from one SRS to another.
    * It will also allow the inverse operation if it exists.
    * 
    * Create an SRSOperation with the SRS::to() method.
    */
    class ROCKY_EXPORT SRSOperation
    {
    public:
        //! Construct an empty (invalid) operation
        SRSOperation();

        //! Whether this is a valid and legal operation
        bool valid() const;

        //! Source SRS of the operation
        const SRS& from() const {
            return _from;
        }

        //! Target SRS of the operation
        const SRS& to() const {
            return _to;
        }

        //! Transform a 3-vector
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        bool transform(const DVEC3A& in, DVEC3B& out) const {
            out.x = in.x, out.y = in.y, out.z = in.z;
            return forward(get_handle(), out.x, out.y, out.z);
        }

        //! Transform a 3-vector (symonym for transform() method)
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        bool operator()(const DVEC3A& in, DVEC3B& out) const {
            out.x = in.x, out.y = in.y, out.z = in.z;
            return forward(get_handle(), out.x, out.y, out.z);
        }

        //! Transform a range of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename ITERATOR>
        bool transformRange(ITERATOR begin, ITERATOR end) const {
            unsigned errors = 0;
            void* handle = get_handle();
            for (auto iter = begin; iter != end; ++iter)
                if (!forward(handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        //! Inverse-transform a 3-vector
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        bool inverse(const DVEC3A& in, DVEC3B& out) const {
            out = in;
            return inverse(get_handle(), out.x, out.y, out.z);
        }

        //! Inverse-transform a range of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename ITERATOR>
        bool inverseRange(ITERATOR begin, ITERATOR end) const {
            unsigned errors = 0;
            void* handle = get_handle();
            for (auto iter = begin; iter != end; ++iter)
                if (!inverse(handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        // copy/move ops
        SRSOperation(const SRSOperation& rhs) = default;
        SRSOperation& operator=(const SRSOperation&) = default;
        SRSOperation(SRSOperation&& rhs) { *this = rhs; }
        SRSOperation& operator=(SRSOperation&&);
        ~SRSOperation();

    private:
        SRS _from;
        SRS _to;

        void* get_handle() const;
        bool forward(void* handle, double& x, double& y, double& z) const;
        bool inverse(void* handle, double& x, double& y, double& z) const;

        friend class SRS;
    };
}
