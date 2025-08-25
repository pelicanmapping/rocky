/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Units.h>
#include <rocky/Ellipsoid.h>
#include <optional>
#include <limits>

namespace ROCKY_NAMESPACE
{
    class SRSOperation;

    /**
    * Spatial reference system.
    * An SRS is the context that makes coordinates geospatially meaningful.
    */
    class ROCKY_EXPORT SRS
    {
    public:
        //! Longitude and Latitude on the WGS84 ellipsoid (degrees)
        //! https://en.wikipedia.org/wiki/World_Geodetic_System
        static const SRS WGS84;

        //! Earth Centered Earth Fixed (Geocentric cartesian, meters)
        //! https://en.wikipedia.org/wiki/Earth-centered,_Earth-fixed_coordinate_system
        static const SRS ECEF;

        //! Spherical Mercator, most common SRS for web maps (meters)
        //! https://proj.org/operations/projections/merc.html
        static const SRS SPHERICAL_MERCATOR;

        //! Equidistant Cylindrical (meters)
        //! https://proj.org/operations/projections/eqc.html
        static const SRS PLATE_CARREE;

        //! Earth's Moon, Geographic coords (degrees)
        static const SRS MOON;

        //! Empty invalid SRS
        static const SRS EMPTY;

    public:
        //! Construct an empty (invalid) SRS.
        SRS() = default;

        //! Construct a new SRS from defintion strings.
        //! 
        //! @param definition Definition string may be WKT string, 
        //!     a PROJ init string, an epsg code (like epsg:4326), or a
        //!     well-known alias like "wgs84" or "spherical-mercator".
        SRS(std::string_view definition);

        //! Make an operation that will take coordinates from this SRS to another
        //! @param target Target SRS for coordinate operation
        //! @return Operation that will transform coordinates from this SRS to the target SRS
        SRSOperation to(const SRS& target) const;

        //! Name of this SRS
        //! @return Name of the SRS, or an empty string if the SRS is invalid
        const char* name() const;

        //! Definition that was used to initialize this SRS
        //! @return Definition string
        inline const std::string& definition() const {
            return _definition;
        }

        //! Whether this is a valid SRS
        //! @return True if the SRS is valid, false if not
        inline bool valid() const;            

        //! Whether this is a valid SRS
        //! @return True if the SRS is valid, false if not
        inline operator bool() const {
            return valid();
        }

        //! Is this a geodetic (long, lat) SRS?
        //! @return True if the SRS is geodetic, false if not
        bool isGeodetic() const;

        //! Is this projected (XY) SRS?
        //! @return True if the SRS is projected, false if not
        bool isProjected() const;

        //! Is this a geocentric (ECEF) SRS?
        //! @return True if the SRS is geocentric, false if not
        bool isGeocentric() const;

        //! Is this a Quadrilateralized Spherical Cube SRS?
        //! @return True if the SRS is a Quadrilateralized Spherical Cube, false if not
        bool isQSC() const;

        //! Has a vertical datum shift?
        //! @return True if the SRS has a vertical datum shift, false if not
        bool hasVerticalDatumShift() const;

        //! Gets the underlying geodetic (longitude, latitude) SRS
        //! @return Geodetic SRS, which will be "this" if this is already geodetic
        const SRS& geodeticSRS() const;

        //! Gets the corresponding geocentric SRS. Only applies to a geodetic SRS.
        //! @return Geocentric SRS, which will be "this" if this is already geocentric,
        //!   or an empty SRS if this is not geodetic.
        const SRS& geocentricSRS() const;

        //! WKT (OGC Well-Known Text) representation
        //! @return WKT string, or an empty string if the SRS is invalid
        const std::string& wkt() const;

        //! Units of measure for the horizontal components
        //! @return Units of measure
        const Units& units() const;

        //! Underlying reference ellipsoid
        //! @return Reference ellipsoid
        const Ellipsoid& ellipsoid() const;

        //! Bounding box, if known
        //! @return Bounding box, or an empty box if the SRS is invalid
        const Box& bounds() const;

        //! Geodetic bounding box, if known
        //! @return Geodetic bounding box, or an empty box if the SRS is invalid
        const Box& geodeticBounds() const;

        //! Whether this SRS is mathematically equivalent to another SRS
        //! without taking vertical datums into account.
        //! @param rhs SRS to compare against
        //! @return True if the SRS are equivalent, false if not
        bool horizontallyEquivalentTo(const SRS& rhs) const;

        //! Whether this SRS is mathematically equivalent to another SRS
        //! @param rhs SRS to compare against
        //! @return True if the SRS are equivalent, false if not
        bool equivalentTo(const SRS& rhs) const;

        //! Whether this SRS is mathematically equivalent to another SRS
        //! @param rhs SRS to compare against
        //! @return True if the SRS are equivalent, false if not
        inline bool operator == (const SRS& rhs) const {
            return equivalentTo(rhs);
        }

        //! Whether this SRS is NOT mathematically different from another SRS
        //! @param rhs SRS to compare against
        //! @return True if the SRS are NOT equivalent, false otherwise
        inline bool operator != (const SRS& rhs) const {
            return !equivalentTo(rhs);
        }

        //! Make a matrix that will transform coordinates from a topocentric
        //! ENU coordinate system (a local tangent plane) centered at
        //! the provided origin into cartesian world coordinates (geocentric if
        //! the SRS is geographic; projected if the SRS is projected).
        //! (Note: if this is a geographic SRS, the local tangent plane will
        //! be in geocentric cartesian space.)
        //! @param origin Origin of the local tangent plane in world coordintes
        //! @return Matrix that will transform ENU coordinates to world coordinates
        glm::dmat4 topocentricToWorldMatrix(const glm::dvec3& origin) const;

        //! Transform a value expressed in fromSRS' base units to the correspond value
        //! expressed in toSRS' units, using the latitude if necessary
        //! @param input Value to transform
        //! @param fromSRS SRS whose base units present the input
        //! @param toSRS SRS whose base units are desired
        //! @param latitude Latitude to use in the transformation if neccesary
        //! @return Transformed value
        static double transformUnits(
            double input,
            const SRS& fromSRS,
            const SRS& toSRS,
            const Angle& latitude);

        //! Transform a value expressed in fromSRS' base units to the correspond value
        //! expressed in toSRS' units, using the latitude if necessary
        //! @param distance Value to transform
        //! @param outSRS SRS whose base units are desired
        //! @param latitude Latitude to use in the transformation if neccesary
        //! @return Transformed value
        static double transformUnits(
            const Distance& distance,
            const SRS& outSRS,
            const Angle& latitude);

        //! Transform a distance from one SRS to another, with an optional reference latitude.
        //! @param distance Distance to transform
        //! @param output_units Units of the output distance
        //! @param latitude Latitude to use in the transformation if neccesary
        double transformDistance(
            const Distance& distance,
            const Units& output_units,
            const Angle& latitude = { 0 }) const;

        //! If the event of an error, return the last error message
        //! @return Error message, or an empty string if there was no error
        const std::string& errorMessage() const;

        // copy/move operations
        SRS(const SRS& rhs) = default;
        SRS& operator=(const SRS& rhs) = default;
        SRS(SRS&&) = default;
        SRS& operator =(SRS&& rhs) = default;

        //! Internal SRS representation (for debugging)
        std::string string() const;

        //! Version of PROJ we use
        //! @return Version string
        static std::string projVersion();

        //! PROJ message redirector
        static std::function<void(int level, const char* msg)> projMessageCallback;

    private:
        //! Create an SRS from an initialization string.
        std::string _definition;
        mutable std::optional<bool> _valid;
        mutable std::optional<int> _crs_type;
        friend class SRSOperation;

        bool _establish_valid() const;
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
        //! @return True if the operation is valid, false if not
        bool valid() const {
            return _handle != nullptr;
        }

        //! Whether this is a valid and legal operation
        //! @return True if the operation is valid, false if not
        inline operator bool() const {
            return valid();
        }

        //! Whether this operation is a no-op.
        //! @return True if the operation is a no-op, false if not
        inline bool noop() const {
            return _nop;
        }

        //! Source SRS of the operation
        //! @return Source SRS
        inline const SRS& from() const {
            return _from;
        }

        //! Target SRS of the operation
        //! @return Target SRS
        inline const SRS& to() const {
            return _to;
        }

        //! Transform a 2D point
        //! @return True is the transformation succeeded
        inline bool transform(double& x, double& y) const {
            double unused = 0.0;
            return _nop ? true : forward(_handle, x, y, unused);
        }

        //! Transform a 3D point
        //! @return True is the transformation succeeded
        inline bool transform(double& x, double& y, double& z) const {
            return _nop ? true : forward(_handle, x, y, z);
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

        //! Transform a 3-vector (symonym for transform() method)
        //! @return Transfomed result, or NANs on error.
        template<typename DVEC3>
        inline DVEC3 operator()(const DVEC3& in) const {
            if (_nop) return in;
            DVEC3 out(in);
            if (forward(_handle, out.x, out.y, out.z)) return out;
            else return DVEC3(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
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

        //! Inverse-transform a 2D point
        //! @return True is the transformation succeeded
        inline bool inverse(double& x, double& y) const {
            double z = 0.0;
            return _nop ? true : inverse(_handle, x, y, z);
        }

        //! Inverse-transform a 3D point
        //! @return True is the transformation succeeded
        inline bool inverse(double& x, double& y, double& z) const {
            return _nop ? true : inverse(_handle, x, y, z);
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

        //! Transform a bounding box from this SRS to the target SRS.
        //! @param input Input bounds
        //! @return output bounds
        Box transformBounds(const Box& input) const;

        //! Given a point in the source SRS, modify it such that its transformed
        //! coordinates would be clamped within the bounds of the target SRS.
        //! (NB: this method does not actually perform the transformation to the
        //! target SRS; the points remain int the source SRS.)
        //! @param x X coordinate to clamp
        //! @param y Y coordinate to clamp
        //! @return True if the point was altered by the clamping.
        bool clamp(double& x, double& y) const;

        //! Error message if something returns false
        const std::string& errorMessage() const;

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

        bool forward(void* handle, double& x, double& y, double& z) const;
        bool inverse(void* handle, double& x, double& y, double& z) const;

        bool forward(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const;
        bool inverse(void* handle, double* x, double* y, double* z, std::size_t stride, std::size_t count) const;
        friend class SRS;
    };


    inline bool SRS::valid() const {
        if (_valid.has_value())
            return _valid.value();
        else
            return _establish_valid();
    }
}
