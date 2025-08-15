/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoPoint.h>
#include <rocky/GeoCircle.h>

namespace ROCKY_NAMESPACE
{
    /**
     * An axis-aligned geospatial extent. A bounding box that is aligned with a
     * spatial reference's coordinate system.
     */
    class ROCKY_EXPORT GeoExtent
    {
    public:
        //! Default ctor creates an invalid extent
        GeoExtent();
        GeoExtent(const GeoExtent& rhs) = default;
        GeoExtent& operator=(const GeoExtent&) = default;
        GeoExtent(GeoExtent&& rhs) noexcept = default;
        GeoExtent& operator=(GeoExtent&&) noexcept = default;

        /** Contructs a valid extent */
        GeoExtent(
            const SRS& srs,
            double west, double south,
            double east, double north);

        /** Contructs an invalid extent that you can grow with the expandToInclude method */
        GeoExtent(const SRS& srs);

        /** create from Bounds object */
        GeoExtent(
            const SRS& srs,
            const Box& bounds);

        //! Set from the SW and NE corners.
        void set(double west, double south, double east, double north);

        //! Whether extents are equal. Since the extent is a 2D concept,
        //! the SRS vertical datum is NOT considered.
        bool operator == (const GeoExtent& rhs) const;

        //! Whether extents are unequal. Since the extent is a 2D concept,
        //! the SRS vertical datum is NOT considered.
        bool operator != (const GeoExtent& rhs) const;

        /** Gets the spatial reference system underlying this extent. */
        const SRS& srs() const { return _srs; }

        //! Coordinates of the bounding edges, normalized for the lat/long frame if necessary
        inline double west() const { return _west; }
        inline double east() const { return normalizeX(_west + _width); }
        inline double south() const { return _south; }
        inline double north() const { return _south + _height; }

        //! Coordinates of the bounds, NOT normalized to the lat/long frame.
        inline double xmin() const { return _west; }
        inline double xmax() const { return _west + _width; }
        inline double ymin() const { return _south; }
        inline double ymax() const { return _south + _height; }

        //! East-to-west span of the extent
        inline double width() const { return _width; }

        //! East-to-est span in specified units
        double width(const Units& units) const;

        //! North-to-south span of the extent
        inline double height() const { return _height; }

        //! North-to-south span in specified units
        double height(const Units& units) const;

        //! Gets the centroid of the bounds
        GeoPoint centroid() const;

        //! Legacy @deprecated
        bool getCentroid(double& out_x, double& out_y) const;

        //! True if the extent is geographic and crosses the 180 degree meridian.
        bool crossesAntimeridian() const;

        //! Raw bounds of the extent (unnormalized)
        void getBounds(double& xmin, double& ymin, double& xmax, double& ymax) const;

        //! True if this object defines a real, valid extent with positive area
        inline bool valid() const
        {
            return _srs.valid() && _width >= 0.0 && _height >= 0.0;
        }

        //! If this extent crosses the international date line, populates two extents, one for
        //! each side, and returns true. Otherwise returns false and leaves the reference
        //! parameters untouched.
        bool splitAcrossAntimeridian(GeoExtent& first, GeoExtent& second) const;

        //! Returns this extent transformed into another spatial reference. The result will be
        //! a minimum bounding rectangle.
        GeoExtent transform(const SRS& to_srs) const;

        //! Returns true if the specified point falls within the bounds of the extent.
        //! @param x, y Coordinates to test
        //! @param xy_srs SRS of input x and y coordinates; if null, the method assumes x and y
        //!    are in the same SRS as this object.
        bool contains(double x, double y, const SRS& srs = { }) const;

        //! Returns true if the point falls within this extent.
        bool contains(const GeoPoint& rhs) const;

        //! Returns true if this extent fully contains another extent.
        bool contains(const GeoExtent& rhs) const;

        //! True if this extent fully contains the target bounds.
        bool contains(const Box& rhs) const;

        //! Returns TRUE if this extent intersects another extent.
        //! @param[in ] rhs      Extent against which to perform intersect test
        //! @param[in ] checkSRS If false, assume the extents have the same SRS (don't check).
        bool intersects(const GeoExtent& rhs, bool checkSRS = true) const;

        //! Copy of the anonymous bounding box
        Box bounds() const;

        //! Gets a geo circle bounding this extent.
        GeoCircle computeBoundingGeoCircle() const;

        //! Grow this extent to include the specified point (which is assumed to be
        //! in the extent's SRS.
        void expandToInclude(double x, double y);

        //! Expand this extent to include the bounds of another extent.
        bool expandToInclude(const GeoExtent& rhs);

        //! Intersect this extent with another extent in the same SRS and return the result.
        GeoExtent intersectionSameSRS(const GeoExtent& rhs) const;

        //! Returns a human-readable string containing the extent data (without the SRS)
        std::string toString() const;

        //! Inflates this GeoExtent by the given ratios
        void scale(double x_scale, double y_scale);

        //! Expands the extent by x and y.
        void expand(double x, double y);

        //! Expands the extent by x and y.
        void expand(const Distance& x, const Distance& y);

        //! Gets the area of this GeoExtent in square meters.
        double area() const;

        //! Computes a scale/bias matrix that transforms parametric coordinates [0..1]
        //! from this extent into the target extent. Return false if the extents are
        //! incompatible (different SRS, etc.)
        //! 
        //! For example, if this extent is 100m wide, and the target extent is
        //! 200m wide, the output matrix will have an x_scale = 0.5.
        //! 
        //! Note! For the sake of efficiency, this method does NOT check for
        //! validity nor for SRS equivalence -- so be sure to validate those beforehand.
        //! It also assumes the output matrix is preset to the identity.
        bool createScaleBias(const GeoExtent& target, glm::dmat4& output) const;

        //! Generates a Sphere encompassing the extent and a vertical volume, in world coordinates.
        Sphere createWorldBoundingSphere(double minElev, double maxElev) const;

        //! Clamps a 2D point to this extent.
        template<typename VEC>
        inline VEC clamp(const VEC& vec) const;

        template<typename ITER>
        inline void clamp(ITER begin, ITER end) const;

    public:
        static GeoExtent INVALID;

    private:
        double _west, _width, _south, _height;
        SRS _srs;

        double normalizeX(double longitude) const;

        void clamp();

        void setOriginAndSize(double west, double south, double width, double height);
    };


    /**
     * A geospatial area with tile data LOD extents
     */
    class ROCKY_EXPORT GeoExtent3D : public GeoExtent
    {
    public:
        GeoExtent3D() = default;
        GeoExtent3D(const GeoExtent& extent) : GeoExtent(extent) { }
        GeoExtent3D(const GeoExtent& extent, const std::string& in_description) : GeoExtent(extent) {
            description = in_description;
        }
        GeoExtent3D(const GeoExtent& extent, unsigned in_minLevel) : GeoExtent(extent) {
            minLevel = in_minLevel;
        }
        GeoExtent3D(const GeoExtent& extent, unsigned in_minLevel, const std::string& in_description) : GeoExtent(extent) {
            minLevel = in_minLevel;
            description = in_description;
        }
        GeoExtent3D(const GeoExtent& extent, unsigned in_minLevel, unsigned in_maxLevel) : GeoExtent(extent) {
            minLevel = in_minLevel;
            maxLevel = in_maxLevel;
        }
        GeoExtent3D(const GeoExtent& extent, unsigned in_minLevel, unsigned in_maxLevel, const std::string& in_description) : GeoExtent(extent) {
            minLevel = in_minLevel;
            maxLevel = in_maxLevel;
            description = in_description;
        }

        //! The minimum LOD of the extent
        option<unsigned> minLevel = 0u;

        //! The maximum LOD of the extent
        option<unsigned> maxLevel = 99u;

        //! description for the data extents
        option<std::string> description;
    };

    using DataExtent = GeoExtent3D;
    using DataExtentList = std::vector<DataExtent>;



    template<typename VEC>
    inline VEC GeoExtent::clamp(const VEC& input) const {
        VEC output = input;
        output.x = util::clamp(input.x, xmin(), xmax());
        output.y = util::clamp(input.y, ymin(), ymax());
        return output;
    }

    template<typename ITER>
    inline void GeoExtent::clamp(ITER begin, ITER end) const {
        for (auto it = begin; it != end; ++it) {
            it->x = util::clamp(it->x, xmin(), xmax());
            it->y = util::clamp(it->y, ymin(), ymax());
        }
    }

}
