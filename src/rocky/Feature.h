/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
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
#include <variant>
#include <cmath>

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
        inline Geometry(Type);
        
        //! Constructor (with inline points)
        //! @param type Geometry type
        //! @param pts Points to move into the geometry
        inline Geometry(Type type, std::vector<glm::dvec3>&& pts);

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

    private:
        //! Iterates over geometry parts.
        //! Includes "this" if not empty.
        template<class T = Geometry>
        class iterator_base
        {
        public:
            inline iterator_base(T& g, bool traverse_polygon_holes = true);
            bool hasMore() const { return _next != nullptr; }
            inline T& next();
            template<typename CALLABLE> inline void eachPart(CALLABLE&& func);
            template<typename CALLABLE> inline void eachPart(CALLABLE&& func) const;

        private:
            std::queue<T*> _queue;
            T* _next = nullptr;
            bool _traverse_multi = true;
            bool _traverse_polygon_holes = true;
            inline void fetch();
        };

    public:
        using iterator = iterator_base<Geometry>;
        using const_iterator = iterator_base<const Geometry>;

        //! Visit each part of the geometry, including "this".
        template<typename CALLABLE>
        void eachPart(CALLABLE&& func) {
            iterator(*this, true).eachPart(std::forward<CALLABLE>(func));
        }

        //! Visit each part of the geometry, including "this".
        template<typename CALLABLE>
        void eachPart(CALLABLE&& func) const {
            const_iterator(*this, true).eachPart(std::forward<CALLABLE>(func));
        }
            
        //! Attempt to convert this geometry to a different type
        void convertToType(Type type);

        //! Weather the point is contained in the 2D geometry.
        //! Only applicable to polygons
        bool contains(double x, double y) const;

        //! Readable name of the geometry type
        static std::string typeToString(Type type);
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
        using FieldValueUnion = std::variant<
            std::monostate,
            std::string,
            double,
            long long,
            bool>;

        struct FieldValue : public FieldValueUnion
        {
            inline bool valid() const;
            inline std::string stringValue() const;
            inline double doubleValue() const;
            inline long long intValue() const;
            inline bool boolValue() const;

            inline bool operator == (const std::string& rhs) const {
                return stringValue() == rhs;
            }
            inline bool operator == (const char* rhs) const {
                return stringValue() == rhs;
            }
            inline bool operator == (double rhs) const {
                return doubleValue() == rhs;
            }
            inline bool operator == (long long rhs) const {
                return intValue() == rhs;
            }
            inline bool operator == (bool rhs) const {
                return boolValue() == rhs;
            }
        };

        //! attribute field type
        enum class FieldType
        {
            String,
            Double,
            Int,
            Bool
        };

        using Fields = util::vector_map<std::string, FieldValue>;

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

        //! Construct a feature with the given geometry, SRS, and interpolation
        Feature(const SRS& in_srs, Geometry::Type type, std::vector<glm::dvec3>&& pts, GeodeticInterpolation interp = GeodeticInterpolation::GreatCircle) :
            srs(in_srs),
            geometry(type, std::move(pts)),
            interpolation(interp)
        {
            dirtyExtent();
        }

        //! Whether the feature is valid
        inline bool valid() const {
            return srs.valid();
        }

        //! Whether the feature contains the named field
        inline bool hasField(const std::string& name) const {
            return fields.find(name) != fields.end();
        }

        //! Reference to the named field, or a dummy empty field if the field is not found.
        inline const FieldValue& field(const std::string& name) const {
            static FieldValue empty;
            auto i = fields.find(name);
            return i != fields.end() ? i->second : empty;
        }

        //! Transform this feature to a different SRS, in place.
        bool transformInPlace(const SRS& to_srs);

        //! Call this is the alter the geometry to recalculate the extent.
        void dirtyExtent();
    };

    /**
    * Interface/base class for factories for Feature objects.
    */
    class ROCKY_EXPORT FeatureSource : public rocky::Inherit<Object, FeatureSource>
    {
    public:
        //! Metadata about a FeatureSource
        struct Metadata
        {
            GeoExtent extent;
            std::vector<std::string> fieldNames;
        };

        //! Iterator that returns features
        class iterator
        {
        public:
            struct implementation {
                virtual bool hasMore() const = 0;
                virtual Feature next() = 0;
            };

        public:
            bool hasMore() const { return _impl->hasMore(); }
            Feature next() { return _impl->next(); }
            template<typename CALLABLE> inline void each(CALLABLE&& func);
            iterator(implementation* impl) : _impl(impl) { }

        private:
            std::unique_ptr<implementation> _impl;
        };

        //! Number of features, or -1 if the count isn't available
        virtual int featureCount() const = 0;

        //! Creates a feature iterator
        virtual iterator iterate(const IOOptions& io) = 0;

        //! Iterate over all features with a callable function with the signature
        //! void(Feature&&)
        template<typename CALLABLE>
        void each(const IOOptions& io, CALLABLE&& func) {
            iterate(io).each(std::forward<CALLABLE>(func));
        }
    };


    // ---- inline functions ----

    Geometry::Geometry(Type in_type) :
        type(in_type)
    {
        //nop
    }

    Geometry::Geometry(Type in_type, std::vector<glm::dvec3>&& pts) :
        type(in_type),
        points(std::move(pts))
    {
        // nop
    }

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

    // template inlines
    template<class T>
    Geometry::iterator_base<T>::iterator_base(T& geom, bool trav_holes) {
        _traverse_polygon_holes = trav_holes;
        _queue.push(&geom);
        fetch();
    }

    template<class T>
    T& Geometry::iterator_base<T>::next() {
        T* n = _next;
        fetch();
        return *n;
    }

    template<class T>
    void Geometry::iterator_base<T>::fetch() {
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

    template<class T>
    template<typename CALLABLE>
    void Geometry::iterator_base<T>::eachPart(CALLABLE&& func) {
        static_assert(std::is_invocable_r_v<void, CALLABLE, Geometry&>);
        while (hasMore())
            func(next());
    }

    template<class T>
    template<typename CALLABLE>
    void Geometry::iterator_base<T>::eachPart(CALLABLE&& func) const {
        static_assert(std::is_invocable_r_v<void, CALLABLE, const Geometry&>);
        while (hasMore())
            func(next());
    }

    bool Feature::FieldValue::valid() const
    {
        return !std::holds_alternative<std::monostate>(*this);
    }

    std::string Feature::FieldValue::stringValue() const
    {
        if (std::holds_alternative<std::string>(*this))
            return std::get<std::string>(*this);
        else if (std::holds_alternative<double>(*this))
            return std::to_string(std::get<double>(*this));
        else if (std::holds_alternative<long long>(*this))
            return std::to_string(std::get<long long>(*this));
        else if (std::holds_alternative<bool>(*this))
            return std::get<bool>(*this) ? "true" : "false";
        return {};
    }

    double Feature::FieldValue::doubleValue() const
    {
        if (std::holds_alternative<double>(*this))
            return std::get<double>(*this);
        else if (std::holds_alternative<long long>(*this))
            return static_cast<double>(std::get<long long>(*this));
        else if (std::holds_alternative<std::string>(*this))
            return std::atof(std::get<std::string>(*this).c_str());
        else if (std::holds_alternative<bool>(*this))
            return std::get<bool>(*this) ? 1.0 : 0.0;
        return 0.0;
    }

    long long Feature::FieldValue::intValue() const
    {
        if (std::holds_alternative<long long>(*this))
            return std::get<long long>(*this);
        else if (std::holds_alternative<double>(*this))
            return static_cast<long long>(std::get<double>(*this));
        else if (std::holds_alternative<std::string>(*this))
            return std::atoll(std::get<std::string>(*this).c_str());
        else if (std::holds_alternative<bool>(*this))
            return std::get<bool>(*this) ? 1LL : 0LL;
        return 0LL;
    }

    bool Feature::FieldValue::boolValue() const
    {
        if (std::holds_alternative<bool>(*this))
            return std::get<bool>(*this);
        else if (std::holds_alternative<double>(*this))
            return std::get<double>(*this) != 0.0;
        else if (std::holds_alternative<long long>(*this))
            return std::get<long long>(*this) != 0LL;
        else if (std::holds_alternative<std::string>(*this))
            return std::get<std::string>(*this) == "true";
        return false;
    }


    template<typename CALLABLE>
    inline void FeatureSource::iterator::each(CALLABLE&& func) {
        static_assert(std::is_invocable_r_v<void, CALLABLE, Feature&&>);
        while (hasMore())
            func(std::move(next()));
    }
}
