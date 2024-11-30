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
        //! @param definition Definition string may be WKT string, 
        //!     a PROJ init string, an epsg code (like epsg:4326), or a
        //!     well-known alias like "wgs84" or "spherical-mercator".
        SRS(const std::string& definition);

        //! Make an operation that will take coordinates from this SRS to another
        //! @param target Target SRS for coordinate operation
        SRSOperation to(const SRS& target) const;

        //! Name of this SRS
        const char* name() const;

        //! Definition used to initialize this SRS
        inline const std::string& definition() const {
            return _definition;
        }

        //! Whether this is a valid SRS
        inline bool valid() const {
            return _valid;
        }
        inline operator bool() const {
            return valid();
        }

        //! Is this a geodetic (long, lat) SRS?
        bool isGeodetic() const;

        //! Is this projected (XY) SRS?
        bool isProjected() const;

        //! Is this a geocentric (ECEF) SRS?
        bool isGeocentric() const;

        //! Has a vertical datum shift?
        bool hasVerticalDatumShift() const;

        //! Gets the underlying geodetic (longitude, latitude) SRS
        const SRS& geodeticSRS() const;

        //! Gets the corresponding geocentric SRS. Only applies to a geodetic SRS.
        const SRS& geocentricSRS() const;

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
        bool horizontallyEquivalentTo(const SRS& rhs) const;

        //! Whether this SRS is mathematically equivalent to another SRS
        bool equivalentTo(const SRS& rhs) const;

        //! Whether this SRS is mathematically equivalent to another SRS
        inline bool operator == (const SRS& rhs) const {
            return equivalentTo(rhs);
        }
        inline bool operator != (const SRS& rhs) const {
            return !equivalentTo(rhs);
        }

        //! Make a matrix that will transform coordinates from a topocentric
        //! ENU coordinate system (e.g., a local tangent plane) centered at
        //! the provided origin into cartesian world coordinates (geocentric if
        //! the SRS is geographic; projected if the SRS is projected).
        //! (Note: in this is a geographic SRS, the LTP will
        //! be in geocentric cartesian space.)
        glm::dmat4 localToWorldMatrix(const glm::dvec3& origin) const;

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
        SRS(SRS&&) = default;
        SRS& operator =(SRS&& rhs) = default;

        //! Internal SRS representation (for debugging)
        std::string string() const;

        //! Version of PROJ we use
        static std::string projVersion();

        //! PROJ message redirector
        static std::function<void(int level, const char* msg)> projMessageCallback;

    private:
        //! Create an SRS from an initialization string.
        std::string _definition;
        bool _valid = false;
        bool _isGeodetic = false;
        bool _isGeocentric = false;
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
        //! Construct an empty, NOOP operation.
        SRSOperation() = default;

        //! Construct an operation to transform coordinates from one SRS to another.
        //! @param from Source SRS
        //! @param to Target SRS
        SRSOperation(const SRS& from, const SRS& to);

        //! Whether this is a valid and legal operation
        bool valid() const {
            return _handle != nullptr;
        }
        inline operator bool() const {
            return valid();
        }

        //! Whether this operation is a no-op.
        inline bool noop() const {
            return _nop;
        }

        //! Source SRS of the operation
        inline const SRS& from() const {
            return _from;
        }

        //! Target SRS of the operation
        inline const SRS& to() const {
            return _to;
        }

        //! Transform a 3-vector
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        inline bool transform(const DVEC3A& in, DVEC3B& out) const {
            out[0] = in[0], out[1] = in[1], out[2] = in[2];
            return _nop? true : forward(_handle, out[0], out[1], out[2]);
        }

        //! Transform a 3-vector (symonym for transform() method)
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        inline bool operator()(const DVEC3A& in, DVEC3B& out) const {
            out[0] = in[0], out[1] = in[1], out[2] = in[2];
            return _nop ? true : forward(_handle, out[0], out[1], out[2]);
        }

        //! Transform a range of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename ITERATOR>
        inline bool transformRange(ITERATOR begin, ITERATOR end) const {
            if (_nop) return true;
            unsigned errors = 0;
            for (auto iter = begin; iter != end; ++iter)
                if (!forward(_handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        //! Transform an array of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename DVEC3>
        inline bool transformArray(DVEC3* inout, std::size_t count) const {
            return _nop ? true : forward(_handle,
                &inout[0][0], &inout[0][1], &inout[0][2], sizeof(DVEC3), count);
        }

        //! Inverse-transform a 3-vector
        //! @return True is the transformation succeeded
        template<typename DVEC3A, typename DVEC3B>
        inline bool inverse(const DVEC3A& in, DVEC3B& out) const {
            out = { in[0], in[1], in[2] };
            return _nop ? true : inverse(_handle, out[0], out[1], out[2]);
        }

        //! Inverse-transform a range of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename ITERATOR>
        inline bool inverseRange(ITERATOR begin, ITERATOR end) const {
            if (_nop) return true;
            unsigned errors = 0;
            for (auto iter = begin; iter != end; ++iter)
                if (!inverse(_handle, iter->x, iter->y, iter->z))
                    errors++;
            return errors == 0;
        }

        //! Inverse-transform an array of 3-vectors in place
        //! @return True if all transformations succeeded
        template<typename DVEC3>
        inline bool inverseArray(DVEC3* inout, std::size_t count) const {
            return _nop ? true : inverse(_handle,
                &inout[0][0], &inout[0][1], &inout[0][2], sizeof(DVEC3), count);
        }

        //! Error message if something returns false
        inline const std::string& lastError() const {
            return _lastError;
        }

        //! Gets the internal definition string of this operation
        //! (for debugging purposes)
        std::string string() const;

        // copy/move ops
        SRSOperation(const SRSOperation& rhs) = default;
        SRSOperation& operator=(const SRSOperation&) = default;
        SRSOperation(SRSOperation&& rhs) noexcept = default;
        SRSOperation& operator=(SRSOperation&&) noexcept = default;

    private:
        void* _handle = nullptr;
        bool _nop = true;
        SRS _from, _to;
        mutable std::string _lastError;

        bool forward(void* handle, double& x, double& y, double& z) const;
        bool inverse(void* handle, double& x, double& y, double& z) const;

        bool forward(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const;
        bool inverse(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const;
        friend class SRS;
    };
}
