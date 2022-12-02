/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoExtent.h>
#include <rocky/Heightfield.h>

namespace rocky
{
    class VerticalDatum;

    /**
     * A georeferenced heightfield
     */
    class ROCKY_EXPORT GeoHeightfield
    {
    public:
        //! Constructs an empty (invalid) heightfield.
        GeoHeightfield();

        //! Constructs a new georeferenced heightfield.
        GeoHeightfield(
            shared_ptr<Heightfield> heightField,
            const GeoExtent& extent);

        static GeoHeightfield INVALID;

        //! True if this is a valid heightfield.
        bool valid() const;

        //! Gets the elevation value at a specified point.
        //!
        //! @param srs
        //!      Spatial reference of the query coordinates. (If you pass in NULL, the method
        //!      will assume that the SRS is equivalent to that of the GeoHeightField. Be sure
        //!      this is case of you will get incorrect results.)
        //! @param x, y
        //!      Coordinates at which to query the elevation value.
        //! @param interp
        //!      Interpolation method for the elevation query.
        //! @param srsWithOutputVerticalDatum
        //!      Transform the output elevation value according to the vertical datum
        //!      associated with this SRS. If the SRS is NULL, assume a geodetic vertical datum
        //!      relative to this object's reference ellipsoid.
        //! @param out_elevation
        //!      Output: the elevation value
        //! @return
        //!      True if the elevation query was succesful; false if not (e.g. if the query
        //!      fell outside the geospatial extent of the heightfield)
        bool getElevation(
            shared_ptr<SRS> inputSRS,
            double x,
            double y,
            Image::Interpolation interp,
            shared_ptr<SRS> srsWithOutputVerticalDatum,
            float& out_elevation) const;

        //! Gets the elevation at a point (must be in the same SRS; bilinear interpolation)
        //float getElevation(
        //    double x, double y,
        //    Image::Interpolation interp = Image::BILINEAR) const;

        //! Subsamples the heightfield, returning a new heightfield corresponding to
        //! the destEx extent. The destEx must be a smaller, inset area of sourceEx.
        GeoHeightfield createSubSample(
            const GeoExtent& destEx, 
            unsigned width, unsigned height,
            Image::Interpolation interpolation) const;

        //! Gets the geospatial extent of the heightfield.
        const GeoExtent& getExtent() const;

        //! The minimum height in the heightfield
        float getMinHeight() const { return _minHeight; }

        //! The maximum height in the heightfield
        float getMaxHeight() const { return _maxHeight; }

        //! Gets a pointer to the underlying OSG heightfield.
        shared_ptr<Heightfield> getHeightfield() const {
            return _hf;
        }

        //! Gets the X interval of this GeoHeightField
        inline dvec2 getResolution() const;

        //! Gets the height at a geographic location (in this object's SRS)
        float getHeightAtLocation(
            double x, double y,
            Image::Interpolation interp = Image::BILINEAR) const;

        // Functor to GeoHeightField's by resolution
        struct SortByResolutionFunctor
        {
            inline bool operator() (const GeoHeightfield& lhs, const GeoHeightfield& rhs) const
            {
                return lhs.getResolution().x < rhs.getResolution().x;
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

    inline dvec2 GeoHeightfield::getResolution() const {
        return _resolution;
    }
}
