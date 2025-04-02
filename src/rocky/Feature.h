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
#include <rocky/Utils.h>
#include <vector>
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
        Geometry& operator=(const Geometry& rhs) = default;

        //! Move constructor
        Geometry(Geometry&& rhs) noexcept = default;
        Geometry& operator=(Geometry&& rhs) noexcept = default;
        
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
        template<class T = Geometry>
        class iterator
        {
        public:
            inline iterator(T& g, bool traverse_polygon_holes = true);
            bool hasMore() const { return _next != nullptr; }
            inline T& next();

        private:
            std::queue<T*> _queue;
            T* _next = nullptr;
            bool _traverse_multi = true;
            bool _traverse_polygon_holes = true;
            inline void fetch();
        };

        using const_iterator = iterator<const Geometry>;

        //! Attempt to convert this geometry to a different type
        void convertToType(Type type);

        //! Weather the point is contained in the 2D geometry.
        //! Only applicable to polygons
        bool contains(double x, double y) const;

        //! Readable name of the geometry type
        static std::string typeToString(Type type);
    };

    // template inlines
    template<class T>
    Geometry::iterator<T>::iterator(T& geom, bool trav_holes) {
        _traverse_polygon_holes = trav_holes;
        _queue.push(&geom);
        fetch();
    }

    template<class T>
    T& Geometry::iterator<T>::next() {
        T* n = _next;
        fetch();
        return *n;
    }

    template<class T>
    void Geometry::iterator<T>::fetch() {
        _next = nullptr;
        if (_queue.empty())
            return;
        T* current = _queue.front();
        _queue.pop();
        bool is_multi =
            current->type == Geometry::Type::MultiLineString ||
            current->type == Geometry::Type::MultiPoints ||
            current->type == Geometry::Type::MultiPolygon;

        if (is_multi && _traverse_multi)
        {
            for (auto& part : current->parts)
                _queue.push(&part);
            fetch();
        }
        else
        {
            _next = current;
            if (current->type == Type::Polygon && _traverse_polygon_holes)
            {
                for (auto& ring : current->parts)
                    _queue.push(&ring);
            }
        }
    }


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

        using Fields = util::vector_map<std::string, FieldValueUnion, FieldNameComparator>;

        using FieldSchema = util::vector_map<std::string, FieldType>;

        using ID = long long;


    public:
        ID id = -1LL;
        Geometry geometry;
        Fields fields;
        SRS srs = SRS::WGS84;
        GeoExtent extent;
        GeodeticInterpolation interpolation = GeodeticInterpolation::GreatCircle;

        //! Construct an empty feature object.
        Feature() = default;

        //! Copy constructor
        Feature(const Feature& rhs) = default;
        Feature& operator =(const Feature& rhs) = default;

        //! Move constructor
        Feature(Feature&& rhs) noexcept = default;
        Feature& operator =(Feature&& rhs) noexcept = default;

        //! Whether the feature is valid
        bool valid() const {
            return srs.valid();
        }

        //! Whether the feature contains the named field
        bool hasField(const std::string& name) const {
            return fields.find(name) != fields.end();
        }

        const FieldValueUnion& field(const std::string& name) const {
            static FieldValueUnion empty;
            auto i = fields.find(name);
            return i != fields.end() ? i->second : empty;
        }

        void dirtyExtent();
    };

    /**
    * Extent, SRS, and possibly the tiling profile of a feautre source
    */
    struct FeatureProfile
    {
        GeoExtent extent;
    };

    /**
    * Interface/base class for factories for Feature objects.
    */
    class ROCKY_EXPORT FeatureSource : public rocky::Inherit<Object, FeatureSource>
    {
    public:
        //! Iterator that returns features
        class iterator
        {
        public:
            struct implementation {
                virtual bool hasMore() const = 0;
                virtual const Feature& next() = 0;
            };

        public:
            bool hasMore() const { return _impl->hasMore(); }
            const Feature& next() { return _impl->next(); }

            iterator(implementation* impl) : _impl(impl) { }

        private:
            std::unique_ptr<implementation> _impl;
        };

        //! Number of features, or -1 if the count isn't available
        virtual int featureCount() const = 0;

        //! Creates a feature iterator
        virtual iterator iterate(IOOptions& io) = 0;
    };


#ifdef ROCKY_HAS_GDAL

    /**
    * Reads Feature objects from various sources using the GDAL OGR library.
    */
    class ROCKY_EXPORT GDALFeatureSource : public rocky::Inherit<FeatureSource, GDALFeatureSource>
    {
    public:

        //! URI of source data, like a shapefile or connection string
        option<URI> uri;

        //! Optional name of the specific OGR driver to load.
        option<std::string> ogrDriver;

        //! Use these to create a feature source from an existing OGR layer handle and SRS.
        //! Leave URI empty if you use this method.
        void* externalLayerHandle = nullptr;
        SRS externalSRS = SRS::WGS84;

        //! Opens the source and returns a status indicating success or failure.
        Status open();

        void close();

        //! Create an interator to read features from the source
        FeatureSource::iterator iterate(IOOptions& io) override;

        //! Number of features, or -1 if the count isn't available
        int featureCount() const override;

        std::string layerName;

        // destructor
        virtual ~GDALFeatureSource();

    private:
        void* _dsHandle = nullptr;
        void* _layerHandle = nullptr;
        int _featureCount = -1;
        std::thread::id _dsHandleThreadId;
        FeatureProfile _featureProfile;
        std::string _source;

        class ROCKY_EXPORT iterator_impl : public FeatureSource::iterator::implementation
        {
        public:
            ~iterator_impl();
            bool hasMore() const override;
            const Feature& next() override;
        private:
            std::queue<Feature> _queue;
            Feature _lastFeatureReturned;
            GDALFeatureSource* _source = nullptr;
            void* _dsHandle = nullptr;
            void* _layerHandle = nullptr;
            void* _resultSetHandle = nullptr;
            void* _spatialFilterHandle = nullptr;
            void* _nextHandleToQueue = nullptr;
            bool _resultSetEndReached = true;
            const std::size_t _chunkSize = 500;
            Feature::ID _idGenerator = 1;

            void init();
            void readChunk();
            friend class GDALFeatureSource;
        };
    };

#endif // ROCKY_HAS_GDAL

    // inline implementation
    template<class VEC3_ITER>
    Geometry::Geometry(Type in_type, VEC3_ITER begin, VEC3_ITER end) :
        type(in_type),
        points(begin, end)
    {
        //nop
    }
    template<class VEC3_CONTAINER>
    Geometry::Geometry(Type in_type, const VEC3_CONTAINER& container) :
        type(in_type),
        points(container.begin(), container.end())
    {
        //nop
    }
}
