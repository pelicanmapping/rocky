/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Units.h>
#include <rocky/Ellipsoid.h>
#include <rocky/Threading.h>
#include <unordered_map>

namespace rocky
{
    class VerticalDatum;

    //Definitions for the mercator extent
    const double MERC_MINX = -20037508.34278925;
    const double MERC_MINY = -20037508.34278925;
    const double MERC_MAXX =  20037508.34278925;
    const double MERC_MAXY =  20037508.34278925;
    const double MERC_WIDTH = MERC_MAXX - MERC_MINX;
    const double MERC_HEIGHT = MERC_MAXY - MERC_MINY;

    /** 
     * SRS holds information describing the reference ellipsoid/datum
     * and the projection of geospatial data.
     */
    class ROCKY_EXPORT SRS
    {
    public:
        //! Gets an SRS from two intialization strings;
        //! the first for the horizontal datum and the second for
        //! the vertical datum. If you omit the vertical datum,
        //!it will default to the geodetic datum for the ellipsoid.
        static shared_ptr<SRS> get(
            const std::string& horiz_spec,
            const std::string& vert_spec = "");

    public: // Basic transformations.

        bool transform(
            const dvec3& input,
            const SRS* output_srs,
            dvec3& output) const;

        bool transform(
            const dvec3& input,
            shared_ptr<SRS> output_srs,
            dvec3& output) const
        {
            return transform(input, output_srs.get(), output);
        }

        /**
         * Transform a collection of points from this SRS to another SRS.
         * Returns true if ALL transforms succeeded, false if at least one failed.
         */
        bool transformInPlace(
            std::vector<dvec3>& input,
            const SRS* output_srs) const;

        bool transformInPlace(
            std::vector<dvec3>& input,
            shared_ptr<SRS> output_srs) const
        {
            return transformInPlace(input, output_srs.get());
        }

        /**
         * Transform a 2D point directly. (Convenience function)
         */
        bool transform2D(
            double x, double y,
            const SRS* output_srs,
            double& out_x, double& out_y) const;

        bool transform2D(
            double x, double y,
            shared_ptr<SRS> output_srs,
            double& out_x, double& out_y) const
        {
            return transform2D(x, y, output_srs.get(), out_x, out_y);
        }


    public: // Units transformations.

        /**
         * Transforms a distance from the base units of this SRS to the base units of
         * another. If one of the SRS's is geographic (i.e. has angular units), the 
         * conversion will assume that the corresponding distance is measured at the
         * equator.
         */
        double transformUnits(
            double distance,
            shared_ptr<SRS> outputSRS,
            const Angle& latitude = Angle()) const;

        static double transformUnits(
            const Distance& distance,
            shared_ptr<SRS> outputSRS,
            const Angle& latitude = Angle());


    public: // World transformations.

        /**
         * Transforms a point from this SRS into "world" coordinates. This normalizes
         * the Z coordinate (according to the vertical datum) and converts to geocentric
         * coordinates if necessary.
         */
        bool transformToWorld(
            const dvec3& input,
            dvec3& out_world) const;

        /**
         * Transforms a point from the "world" coordinate system into this spatial
         * reference.
         * @param world
         *      World point to transform
         * @param out_local
         *      Output coords in local (SRS) coords
         * @param out_geodeticZ
         *      (optional) Outputs the geodetic (HAE) Z if applicable
         */
        bool transformFromWorld(
            const dvec3& world,
            dvec3& out_local,
            double* out_geodeticZ =nullptr ) const;

    public: // extent transformations.
        
        //! Transforms a minimum bounding rectangle from this SRS to another.
        //! The operation is not necessarily commutitive.
        virtual bool transformExtentToMBR(
            shared_ptr<SRS> to_srs,
            double& in_out_xmin, 
            double& in_out_ymin,
            double& in_out_xmax, 
            double& in_out_ymax ) const;
        
        //! Transforms a 2D grid of points from this SRS to another.
        virtual bool transformGrid(
            shared_ptr<SRS> to_srs,
            double in_xmin, double in_ymin,
            double in_xmax, double in_ymax,
            double* x, double* y,
            unsigned numx, unsigned numy ) const;


    public: // properties

        /** uniquely identifies an SRS. */
        struct Key {
            Key();
            Key(const std::string& h, const std::string& v, bool geoc);
            bool operator == (const Key& rhs) const;
            bool operator < (const Key& rhs) const;
            std::string horiz, horizLower;
            std::string vert, vertLower;
            bool geocentric;
            std::size_t hash;
        };

        /** True if this is a geographic SRS (lat/long/msl) */
        virtual bool isGeographic() const;

        /** True if this is a geodetic SRS (lat/long/hae) */
        virtual bool isGeodetic() const;

        /** True if this is a projected SRS (i.e. local coordinate system) */
        virtual bool isProjected() const;

        /** True if this is a planet-centered system (geocentric/meters) */
        virtual bool isGeocentric() const;

        /** Tests whether this SRS represents a Mercator projection. */
        bool isMercator() const;

        /** Tests whether this SRS represents a Spherical Mercator pseudo-projection. */
        bool isSphericalMercator() const;

        /** Tests whether this SRS represents a polar sterographic projection. */
        bool isNorthPolar() const;
        bool isSouthPolar() const;

        /** Tests whether this SRS is user-defined; i.e. whether it is other than a
            well-known SRS. (i.e. whether the SRS is unsupported by GDAL) */
        virtual bool isUserDefined() const;

        /** Tests whether this SRS is a Unified Cube projection (rocky-internal) */
        virtual bool isCube() const;

        /** Tests whether this SRS is a Local Tangent Plane projection (rocky-internal) */
        virtual bool isLTP() const { return _is_ltp; }

        /** Gets the readable name of this SRS. */
        const std::string& getName() const;

        /** Gets the underlying reference ellipsoid of this SRS */
        const Ellipsoid& getEllipsoid() const;

        /** Gets the WKT string */
        const std::string& getWKT() const;

        /** Gets the initialization key. */
        const Key& getKey() const;

        /** Gets the initialization string for the horizontal datum */
        const std::string& getHorizInitString() const;

        /** Gets the initialization string for the vertical datum */
        const std::string& getVertInitString() const;

        /** Gets the datum identifier of this SRS (or empty string if not available) */
        const std::string& getDatumName() const;

        /** Gets the base units of data in this SRS */
        const Units& getUnits() const;

        //! Gets the reported linear unit conversion
        double getReportedLinearUnits() const;

        /** Whether the two SRS are completely equivalent. */
        bool isEquivalentTo(const SRS* rhs) const;
        bool isEquivalentTo(shared_ptr<SRS> rhs) const {
            return isEquivalentTo(rhs.get());
        }

        /** Whether the two SRS are horizonally equivalent (ignoring the vertical datums) */
        bool isHorizEquivalentTo(const SRS* rhs) const;
        bool isHorizEquivalentTo(shared_ptr<SRS> rhs) const {
            return isHorizEquivalentTo(rhs.get());
        }

        /** Whether this SRS has the same vertical datum as another. */
        bool isVertEquivalentTo(const SRS* rhs) const;

        /** Gets a reference to this SRS's underlying geographic SRS. */
        shared_ptr<SRS> getGeographicSRS() const;

        /** Gets a reference to this SRS's underlying geodetic SRS. This is the same as the
            geographic SRS [see getGeographicSRS()] but with a geodetic vertical datum (in
            which Z is expressed as height above the geodetic ellipsoid). */
        shared_ptr<SRS> getGeodeticSRS() const;

        //! Gets the geocentric reference system associated with this SRS' ellipsoid.
        shared_ptr<SRS> getGeocentricSRS() const;

        //! Gets the vertical datum. If null, this SRS uses a default geodetic vertical datum
        shared_ptr<VerticalDatum> getVerticalDatum() const;

        ////! Creates a localizer matrix based on a point in this SRS.
        //bool createLocalToWorld(
        //    const GeoPoint& in,
        //    dmat4& out);

        bool createLocalToWorld(
            const dvec3& point,
            dmat4& out) const;

        /** Create a de-localizer matrix based on a point in this SRS. */
        bool createWorldToLocal(
            const dvec3& point,
            dmat4& out) const;

        /** Creates and returns a local trangent plane SRS at the given reference location.
            The reference location is expressed in this object's SRS, but it tangent to
            the globe at getGeographicSRS(). LTP units are in meters. */
        shared_ptr<SRS> createTangentPlaneSRS(const dvec3& origin) const;

        /** Creates a transverse mercator projection centered at the specified longitude. */
        shared_ptr<SRS> createTransMercFromLongitude(const Angle& lon) const;

        /** Creates a UTM (universal transverse mercator) projection in the UTM zone
            containing the specified longitude. NOTE: this is slightly faster than using
            basic tmerc (transverse mercator) above. */
        shared_ptr<SRS> createUTMFromLonLat(const Angle& lon, const Angle& lat) const;

        /** Create an equirectangular projected SRS corresponding to the geographic SRS
            contained in this spatial reference. This is an approximation of a Plate Carre
            SRS but using equatorial meters. */
        shared_ptr<SRS> createEquirectangularSRS() const;

		//! The underlying OGRLayerH that this SRS owns.
		//! Don't use this unless you know what you're doing - this handle
        //! is thread-specific.
        void* getHandle() const;

        //! Get (a best guess at) the appropriate bounding box for this SRS
        const Box& getBounds() const;

        //! Whether this SRS was successfully initialized and is valid for use
        bool valid() const { return _valid; }

        //! destructor
        virtual ~SRS();

    protected:
        SRS(const Key& key);

        SRS(const SRS&) = delete;

    protected:
        struct TransformInfo {
            TransformInfo() : _failed(false), _handle(nullptr) { }
            bool _failed;
            void* _handle;
        };
        typedef std::unordered_map<std::string,optional<TransformInfo>> TransformHandleCache;

        // SRS requires per-thread handles to be thread safe
        struct ThreadLocalData
        {
            ThreadLocalData();
            ~ThreadLocalData();
            std::thread::id _threadId;
            void* _handle;
            TransformHandleCache _xformCache;
            double* _workspace;
            unsigned _workspaceSize;
        };

        // gets the thread-safe handle, initializing it if necessary
        ThreadLocalData& getThreadLocal() const;

        enum InitType {
            INIT_USER,
            INIT_PROJ,
            INIT_WKT
        };

        struct Setup {
            Setup() : 
                type(INIT_PROJ), 
                srcHandle(nullptr),
                geocentric(false), 
                cube(false) { }
            InitType    type;
            std::string horiz;
            std::string vert;
            std::string name;
            optional<glm::dvec3> originLLA; // for LTP
            bool geocentric;
            bool cube;
            void* srcHandle;
        };

        enum Domain {
            GEOGRAPHIC,
            PROJECTED,
            GEOCENTRIC
        };

        std::string _name;
        Key _key;
        shared_ptr<VerticalDatum> _vdatum;
        Domain _domain;
        std::string _wkt;

        // shortcut bools:
        bool _is_mercator;
        bool _is_spherical_mercator;
        bool _is_north_polar, _is_south_polar;
        bool _is_cube;
        bool _is_user_defined;
        bool _is_ltp;

        unsigned _ellipsoidId;
        std::string _proj4;
        std::string _datum;
        Units _units;
        double _reportedLinearUnits;
        Ellipsoid _ellipsoid;
        mutable shared_ptr<SRS> _geo_srs;
        mutable shared_ptr<SRS> _geodetic_srs;  // _geo_srs with a NULL vdatum.
        mutable shared_ptr<SRS> _geocentric_srs;
        mutable util::Mutex _mutex;
        mutable bool _valid;
        mutable bool _initialized;
        Setup _setup;
        Box _bounds;
        mutable util::ThreadLocal<ThreadLocalData> _local;

        // user can override these methods in a subclass to perform custom functionality; must
        // call the superclass version.
        virtual bool _isEquivalentTo(
            const SRS* srs,
            bool considerVDatum ) const;

        virtual bool preTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const
        {
            return true;
        }

        virtual bool postTransform(
            std::vector<dvec3>& input,
            const SRS** new_srs) const
        {
            return true;
        }

        bool transformXYPointArrays(
            ThreadLocalData& local,
            double* x,
            double* y,
            unsigned numPoints,
            const SRS* out_srs) const;

        bool transformZ(
            std::vector<dvec3>& points,
            const SRS* outputSRS,
            bool pointsAreGeodetic) const;

    private:

        //! initial setup - override to provide custom setup
        void init();

        static shared_ptr<SRS> get(const Key& key);

        friend class Registry;
    };

    //! Convenient symonym
    using SpatialReference = SRS;
}


namespace std {
    // std::hash specialization for SRS::Key
    template<> struct hash<rocky::SRS::Key> {
        inline size_t operator()(const rocky::SRS::Key& value) const {
            return value.hash;
        }
    };
}
