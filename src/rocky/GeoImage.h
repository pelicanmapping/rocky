/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/GeoExtent.h>
#include <rocky/Status.h>
#include <rocky/Threading.h>

namespace rocky
{
    class Image;
    class GeoExtent;

    /**
     * A georeferenced image; i.e. an osg::Image and an associated GeoExtent with SRS.
     */
    class ROCKY_EXPORT GeoImage
    {
    public:

        //! Construct an empty (invalid) geoimage
        GeoImage();

        GeoImage(const GeoImage& rhs) = default;

        //! Construct an image with an error status
        GeoImage(const Status&);

        //! Constructs a new goereferenced image.
        GeoImage(shared_ptr<Image> image, const GeoExtent& extent);

        //! Constructs a new goereferenced image from a future.
        GeoImage(util::Future<shared_ptr<Image>> image, const GeoExtent& extent);

        //! Destructor
        virtual ~GeoImage() { }

        static GeoImage INVALID;

        //! True if this is a valid geo image.
        bool valid() const;

        //! Gets a pointer to the underlying OSG image.
        shared_ptr<Image> getImage() const;

        //! Gets the geospatial extent of the image.
        const GeoExtent& getExtent() const;

        //! Shortcut to get the spatial reference system describing
        //! the projection of the image.
        shared_ptr<SRS> getSRS() const;

        /**
         * Crops the image to a new geospatial extent.
         *
         * @param extent
         *      New extent to which to crop the image.
         * @param exact
         *      If "exact" is true, the output image will have exactly the extents requested;
         *      this process may require resampling and will therefore be more expensive. If
         *      "exact" is false, we do a simple crop of the image that is rounded to the nearest
         *      pixel. The resulting extent will be close but usually not exactly what was
         *      requested - however, this method is faster.
         * @param width, height
         *      New pixel size for the output image. By default, the method will automatically
         *      calculate a new pixel size.
         */
        GeoImage crop(
            const GeoExtent& extent,
            bool exact = false,
            unsigned int width = 0,
            unsigned int height = 0,
            bool useBilinearInterpolation = true) const;

        /**
         * Warps the image into a new spatial reference system.
         *
         * @param to_srs
         *      SRS into which to warp the image.
         * @param to_extent
         *      Supply this extent if you wish to warp AND crop the image in one step. This is
         *      faster than calling reproject() and then crop().
         * @param width, height
         *      New pixel size for the output image. Be default, the method will automatically
         *      calculate a new pixel size.
         */
        GeoImage reproject(
            shared_ptr<SRS> to_srs,
            const GeoExtent* to_extent = nullptr,
            unsigned width = 0,
            unsigned height = 0,
            bool useBilinearInterpolation = true) const;

        /**
         * Returns the underlying OSG image and releases the reference pointer.
         */
        shared_ptr<Image> takeImage();

        /**
         * Gets the units per pixel of this geoimage
         */
        double getUnitsPerPixel() const;

        //! Gets the coordinate at the image's s,t
        bool getCoord(int s, int t, double& out_x, double& out_y) const;

        //! Sets a token object in this GeoImage. You can use this to
        //! keep a weak reference to the object after creating it,
        //! for example to detect destruction.
        void setTrackingToken(shared_ptr<Object> token);
        shared_ptr<Object> getTrackingToken() const;

        //unsigned width() const;
        //unsigned height() const;

        //bool read(pixel_type& output, unsigned s, unsigned t) const {
        //    _read(output, s, t);
        //    return output.r() != NO_DATA_VALUE;
        //}
        //bool read(pixel_type& output, double u, double v) const {
        //    _read(output, u, v);
        //    return output.r() != NO_DATA_VALUE;
        //}

        //ImageUtils::PixelReader& getReader() { return _read; };
        //const ImageUtils::PixelReader& getReader() const { return _read; }

        //! Read the value of a pixel at a geopoint.
        bool read(
            fvec4& output,
            const GeoPoint& p) const;

    private:
        GeoExtent _extent;
        Status _status;
        shared_ptr<Image> _myimage;
        mutable optional<util::Future<shared_ptr<Image>>> _future;
        shared_ptr<Object> _token;
    };

    //typedef std::vector<GeoImage> GeoImageVector;

    //struct GeoImageIterator : public ImageUtils::ImageIteratorWithExtent<GeoExtent>
    //{
    //    GeoImageIterator(const GeoImage& im) :
    //        ImageUtils::ImageIteratorWithExtent<GeoExtent>(
    //            im.getImage(), im.getExtent()) { }
    //};

    //struct GeoImagePixelReader : public ImageUtils::PixelReaderWithExtent<GeoExtent>
    //{
    //    GeoImagePixelReader(const GeoImage& im) :
    //        ImageUtils::PixelReaderWithExtent<GeoExtent>(
    //            im.getImage(), im.getExtent()) { }
    //};
}
