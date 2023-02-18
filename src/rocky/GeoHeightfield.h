/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoCommon.h>
#include <rocky/GeoExtent.h>
#include <rocky/Heightfield.h>

namespace ROCKY_NAMESPACE
{
    /**
     * A georeferenced heightfield
     */
    class ROCKY_EXPORT GeoHeightfield
    {
    public:
        //! Constructs an empty (invalid) heightfield.
        GeoHeightfield();
        GeoHeightfield(const GeoHeightfield&) = default;
        GeoHeightfield& operator=(const GeoHeightfield&) = default;
        GeoHeightfield(GeoHeightfield&& rhs) { *this = rhs; }
        GeoHeightfield& operator=(GeoHeightfield&& rhs);

        //! Constructs a new georeferenced heightfield.
        GeoHeightfield(
            shared_ptr<Heightfield> heightField,
            const GeoExtent& extent);

        static GeoHeightfield INVALID;

        //! True if this is a valid heightfield.
        bool valid() const;

        //! Samples the elevation value at a specified point.
        //! @param x, y
        //!      Coordinates at which to query the elevation value.
        //! @param srs
        //!      Spatial reference of the query coordinates. (If you pass in NULL, the method
        //!      will assume that the SRS is equivalent to that of the GeoHeightField. Be sure
        //!      this is case of you will get incorrect results.)
        //! @param interp
        //!      Interpolation method for the elevation query.
        //! @param srsWithOutputVerticalDatum
        //!      Transform the output elevation value according to the vertical datum
        //!      associated with this SRS. If the SRS is NULL, assume a geodetic vertical datum
        //!      relative to this object's reference ellipsoid.
        //! @return The elevation value, or NO_DATA_VALUE if the query failed
        float heightAt(
            double x, double y,
            const SRS& xy_srs,
            Image::Interpolation interp) const;

        //! Samples the elevation value at a specified point. If you plan to do
        //! multiple samples, this is faster than using heightAt(x, y, srs).
        //! 
        //! @param x, y
        //!      Coordinates at which to query the elevation value.
        //! @param operation
        //!      SRS transformation from the x and y SRS to this heightfield's SRS.
        //! @param interp
        //!      Interpolation method for the elevation query.
        //! @param srsWithOutputVerticalDatum
        //!      Transform the output elevation value according to the vertical datum
        //!      associated with this SRS. If the SRS is NULL, assume a geodetic vertical datum
        //!      relative to this object's reference ellipsoid.
        //! @return The elevation value, or NO_DATA_VALUE if the query failed
        float heightAt(
            double x, double y,
            const SRSOperation& operation,
            Image::Interpolation interp) const;

        //! Subsamples the heightfield, returning a new heightfield corresponding to
        //! the destEx extent. The destEx must be a smaller, inset area of sourceEx.
        GeoHeightfield createSubSample(
            const GeoExtent& destEx, 
            unsigned width, unsigned height,
            Image::Interpolation interpolation) const;

        //! Gets the geospatial extent of the heightfield.
        const GeoExtent& extent() const;

        //! SRS of this heightfield
        const SRS& srs() const { return extent().srs(); }

        //! The minimum height in the heightfield
        float minHeight() const { return _minHeight; }

        //! The maximum height in the heightfield
        float maxHeight() const { return _maxHeight; }

        //! Gets a pointer to the underlying OSG heightfield.
        shared_ptr<Heightfield> heightfield() const {
            return _hf;
        }

        //! Gets the X interval of this GeoHeightField
        inline dvec2 resolution() const;

        //! Gets the height at a geographic location (in this object's SRS)
        float heightAtLocation(
            double x, double y,
            Image::Interpolation interp = Image::BILINEAR) const;

        // Functor to GeoHeightField's by resolution
        struct SortByResolutionFunctor
        {
            inline bool operator() (const GeoHeightfield& lhs, const GeoHeightfield& rhs) const
            {
                return lhs.resolution().x < rhs.resolution().x;
            }
        };

    private:
        GeoExtent _extent;
        shared_ptr<Heightfield> _hf;
        float _minHeight, _maxHeight;
        dvec2 _resolution;

        void init();
    };


    //inline shared_ptr<Heightfield> GeoHeightfield::getHeightfield() const {
    //    return _hf;
    //}

    inline dvec2 GeoHeightfield::resolution() const {
        return _resolution;
    }
}
