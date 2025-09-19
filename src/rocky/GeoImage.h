/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/SRS.h>
#include <rocky/GeoExtent.h>
#include <rocky/Math.h>
#include <rocky/Result.h>
#include <rocky/Image.h>
#include <rocky/Heightfield.h>

namespace ROCKY_NAMESPACE
{
    class Image;
    class GeoPoint;

    /**
     * A georeferenced image; i.e. an Image coupled with a GeoExtent.
     */
    class ROCKY_EXPORT GeoImage
    {
    public:

        //! Construct an empty (invalid) geoimage
        GeoImage();
        GeoImage(const GeoImage&) = default;
        GeoImage& operator=(const GeoImage&) = default;
        GeoImage(const GeoImage&& rhs) noexcept { *this = rhs; }
        GeoImage& operator=(GeoImage&&) noexcept;

        //! Constructs a new goereferenced image.
        GeoImage(std::shared_ptr<Image> image, const GeoExtent& extent);

        //! Destructor
        virtual ~GeoImage() { }

        //! True if this is a valid geo image.
        bool valid() const;
        operator bool() const { return valid(); }

        //! Gets a pointer to the underlying image
        std::shared_ptr<Image> image() const;

        //! Gets the geospatial extent of the image
        const GeoExtent& extent() const;

        //! Shortcut to get the spatial reference system describing
        //! the projection of the image.
        const SRS& srs() const;

        //! Composites one or more source images into this image, overwriting the existing image.
        //! @param sources GeoImages to composite, from bottom to top.
        //! @param opacities Opacities to apply to each source image (defaults to 1.0f if vector sizes don't match)
        void composite(const std::vector<GeoImage>& sources, const std::vector<float>& opacities = {});

        //! Gets the units per pixel of this geoimage
        double getUnitsPerPixel() const;

        //! Gets the coordinate at the image's s,t
        bool getCoord(int s, int t, double& out_x, double& out_y) const;

        //! Gets the pixel at the image's x,y (must be in GeoImage's SRS)
        //! s and t will return -1 if x and y are out of bounds respectively
        bool getPixel(double x, double y, int& s, int& t) const;

        using ReadResult = Result<Image::Pixel, std::nullopt_t>;

        //! Read the value of a pixel at a geopoint.
        ReadResult read(const GeoPoint& p, int layer=0) const;

        //! Read the value of a pixel at the coordinate (x, y) which is
        //! assumed to be in this GeoImage's SRS.
        ReadResult read(double x, double y, int layer=0) const;

        //! Clamp the input coordinate to the image's valid extent and then
        //! read the value of a pixel at the coordinate (x, y) which is
        //! assumed to be in the GeoImage's SRS.
        ReadResult read_clamped(double x, double y, int layer=0) const;

        //! Read the value of a pixel at the coordinate (x, y) which
        //! is expressed in the provided SRS.
        ReadResult read(const SRS& srs, double x, double y, int layer=0) const;

        //! Read the value of a pixel at the coordinate (x, y) which
        //! will be transformed using the provided operation before
        //! reading.
        ReadResult read(const SRSOperation& operation, double x, double y, int layer=0) const;

    private:
        GeoExtent _extent;
        std::shared_ptr<Image> _image;
    };

    class GeoHeightfield
    {
    public:
        using ReadResult = Result<float, std::nullopt_t>;

        GeoHeightfield(const GeoImage& in_image) :
            image(in_image), hf(in_image.image()) {
        }

        const GeoImage& image;
        Heightfield hf;

        ReadResult read(double x, double y) const {
            if (!image.valid()) return ResultFail;
            double u = (x - image.extent().xmin()) / image.extent().width();
            double v = (y - image.extent().ymin()) / image.extent().height();
            if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
                return ResultFail;
            return hf.heightAtUV((float)u, (float)v);
        }
    };
}
