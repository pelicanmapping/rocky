/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Math.h>
#include <rocky/SRS.h>
#include <rocky/URI.h>
#include <rocky/GeoExtent.h>
#include <vector>
#include <stack>
#include <queue>

namespace ROCKY_NAMESPACE
{
    /**
    * Geometric shape data.
    * This class holds one or more lists of points. How these points are interpreted
    * depends on the geometry Type. 
    */
    struct ROCKY_EXPORT Geometry
    {
        //! Types of geometry supported in this class
        enum class Type
        {
            //! A single collection of points (in "points")
            Points,

            //! A single collection of line segments (in "points")
            LineString,

            //! One polygon. "points" contains the outer ring with a CCW winding.
            //! "parts" contains an optional collection of holes, each with a CW winding.
            Polygon,

            //! Each part contains one or more points; "points" is ignored.
            MultiPoints,

            //! Each part contains a separate linestring; "points" is ignored.
            MultiLineString,

            //! Each part contains a separate polygon; "points" is ignored.
            MultiPolygon
        };

        //! How to interpret the geometric data
        Type type = Type::Points;

        //! List of 3D points comprising this geometry. May be empty.
        std::vector<glm::dvec3> points;

        //! Collection of additional parts comprising this geometry, if applicable.
        std::vector<Geometry> parts;

        //! Default constructor
        Geometry() = default;

        //! Constructor
        //! @param type Geometry type
        Geometry(Type);

        //! Copy constructor
        Geometry(const Geometry& rhs) = default;
        
        //! Contruct a typed geometry by copying points from an existing range
        //! @param type Geometry type
        //! @param begin Beginning iterator in range
        //! @param end Ending iterator in range
        template<class VEC3_ITER>
        inline Geometry(Type type, VEC3_ITER begin, VEC3_ITER end);

        //! Contruct a typed geometry from an existing container
        //! @param type Geometry type
        //! @param container Container from which to copy points
        template<class VEC3_CONTAINER>
        inline Geometry(Type type, const VEC3_CONTAINER& container);

        //! Iterates over geometry parts.
        //! Includes "this" if not empty.
        struct ROCKY_EXPORT const_iterator
        {
            const_iterator(const Geometry& g);
            bool hasMore() const;
            const Geometry& next();

        private:
            std::stack<const Geometry*> _stack;
            const Geometry* _next = nullptr;
            bool _traverse_multi = true;
            bool _traverse_polygon_holes = true;
            void fetch();
        };
    };

    /**
    * How to interpolate points along a line segment on a geodetic map
    */
    enum class GeodeticInterpolation
    {
        RhumbLine,
        GreatCircle
    };

    /**
    * GIS Feature couples georeferenced geometry with an attribute field table
    */
    class ROCKY_EXPORT Feature
    {
    public:
        //! internal - raw values for a field
        struct FieldValueUnion
        {
            std::string stringValue = {};
            double doubleValue = 0.0;
            long long intValue = 0;
            bool boolValue = false;

        };

        //! attribute field type
        enum class FieldType
        {
            String,
            Int,
            Double,
            Bool
        };

        using FieldValue = std::pair<FieldType, FieldValueUnion>;

        struct ROCKY_EXPORT FieldNameComparator {
            bool operator()(const std::string& L, const std::string& R) const;
        };

        using Fields = std::map<std::string, FieldValueUnion, FieldNameComparator>;

        using FieldSchema = std::map<std::string, FieldType>;

        using ID = long long;


    public:
        ID id = -1LL;
        Geometry geometry;
        Fields fields;
        SRS srs = SRS::WGS84;
        GeodeticInterpolation interpolation = GeodeticInterpolation::GreatCircle;

        //! Construct an empty feature object.
        Feature() = default;
        Feature(const Feature& rhs) = default;

        bool valid() const {
            return srs.valid(); // && !geometry.empty();
        }
    };


    struct FeatureProfile
    {
        GeoExtent extent;
    };


    class ROCKY_EXPORT FeatureSource : public rocky::Inherit<Object, FeatureSource>
    {
    public:
        class iterator
        {
        public:
            virtual bool hasMore() const = 0;
            virtual const Feature& next() = 0;
        };

        virtual std::shared_ptr<iterator> iterate(IOOptions& io) = 0;
    };


#ifdef GDAL_FOUND

    class ROCKY_EXPORT OGRFeatureSource : public rocky::Inherit<FeatureSource, OGRFeatureSource>
    {
    public:
        ~OGRFeatureSource();

        std::shared_ptr<FeatureSource::iterator> iterate(IOOptions& io) override;

        Status open();
        void close();

        optional<URI> uri;
        optional<std::string> ogrDriver;
        std::string layerName;
        bool writable = false;
        //Profile profile;

    private:
        void* _dsHandle;
        void* _layerHandle;
        std::thread::id _dsHandleThreadId;
        //Geometry::Type _geometryType;
        FeatureProfile _featureProfile;
        std::string _source;

        class ROCKY_EXPORT iterator : public FeatureSource::iterator
        {
        public:
            iterator();
            void init();
            ~iterator();
            bool hasMore() const override;
            const Feature& next() override;
        private:
            std::queue<Feature> _queue;
            Feature _lastFeatureReturned;
            void readChunk();
            friend class OGRFeatureSource;
            OGRFeatureSource* _source = nullptr;
            void* _dsHandle = nullptr;
            void* _layerHandle = nullptr;
            void* _resultSetHandle = nullptr;
            void* _spatialFilterHandle = nullptr;
            void* _nextHandleToQueue = nullptr;
            bool _resultSetEndReached = false;
            const std::size_t _chunkSize = 500;
        };
    };

#endif // GDAL_FOUND

    // inline implementation
    template<class VEC3_ITER>
    Geometry::Geometry(Type in_type, VEC3_ITER begin, VEC3_ITER end) :
        type(in_type),
        points(begin, end)
    {
    }
    template<class VEC3_CONTAINER>
    Geometry::Geometry(Type in_type, const VEC3_CONTAINER& container) :
        type(in_type),
        points(container.begin(), container.end())
    {
    }
}
