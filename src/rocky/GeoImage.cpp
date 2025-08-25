/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoImage.h"
#include "Math.h"
#include "Image.h"
#include "Heightfield.h"

#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_proxy.h>
#include <cpl_string.h>
#include <gdal_vrt.h>
#endif

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

GeoImage::GeoImage() :
    _image(nullptr),
    _extent(GeoExtent::INVALID)
{
    //nop
}

GeoImage&
GeoImage::operator=(GeoImage&& rhs) noexcept
{
    if (this != &rhs)
    {
        _image = std::move(rhs._image);
        _extent = std::move(rhs._extent);
    }
    rhs._image = nullptr;
    rhs._extent = GeoExtent::INVALID;
    return *this;
}

GeoImage::GeoImage(std::shared_ptr<Image> image, const GeoExtent& extent) :
    _image(image),
    _extent(extent)
{
    ROCKY_HARD_ASSERT(image != nullptr && extent.valid());
}

bool
GeoImage::valid() const 
{
    return _extent.valid() && _image != nullptr;
}

std::shared_ptr<Image>
GeoImage::image() const
{
    return _image;
}

const SRS&
GeoImage::srs() const
{
    return _extent.srs();
}

const GeoExtent&
GeoImage::extent() const
{
    return _extent;
}

double
GeoImage::getUnitsPerPixel() const
{
    if (image())
    {
        double uppw = _extent.width() / (double)image()->width();
        double upph = _extent.height() / (double)image()->height();
        return (uppw + upph) / 2.0;
    }
    else return 0.0;
}

bool
GeoImage::getCoord(int s, int t, double& out_x, double& out_y) const
{
    if (!valid()) return false;
    if (!_image) return false;

    double u = (double)s / (double)(_image->width() - 1);
    double v = (double)t / (double)(_image->height() - 1);
    out_x = _extent.xmin() + u * _extent.width();
    out_y = _extent.ymin() + v * _extent.height();
    return true;
}

bool
GeoImage::getPixel(double x, double y, int& s, int& t) const
{
    if (!valid()) return false;
    if (!_image) return false;

    double u = (x - _extent.xmin()) / _extent.width();
    s = (u >= 0.0 && u <= 1.0) ? (int)(u * (double)(_image->width() - 1)) : -1;

    double v = (y - _extent.ymin()) / _extent.height();
    t = (v >= 0.0 && v <= 1.0) ? (int)(v * (double)(_image->height() - 1)) : -1;

    return true;
}

void
GeoImage::composite(const std::vector<GeoImage>& sources, const std::vector<float>& opacities)
{
    double x, y;
    glm::fvec4 pixel;
    bool have_opacities = opacities.size() == sources.size();

    std::vector<SRSOperation> xforms;
    xforms.reserve(sources.size());
    for (auto& source : sources)
        xforms.emplace_back(srs().to(source.srs()));

    for (unsigned s = 0; s < _image->width(); ++s)
    {
        for (unsigned t = 0; t < _image->height(); ++t)
        {
            getCoord(s, t, x, y);

            for (unsigned layer = 0; layer < _image->depth(); ++layer)
            {
                bool pixel_valid = false;

                for (int i = 0; i < (int)sources.size(); ++i)
                {
                    auto& source = sources[i];
                    float opacity = have_opacities ? opacities[i] : 1.0f;
                    auto r = source.read(xforms[i], x, y, layer);
                    if (r.ok())
                    {
                        if (!pixel_valid)
                        {
                            if (pixel.a > 0.0f)
                            {
                                pixel.a *= opacity;
                                pixel_valid = true;
                            }
                            //if (source.read(pixel, xforms[i], x, y, layer) && pixel.a > 0.0f)
                            //{
                            //    pixel.a *= opacity;
                            //    pixel_valid = true;
                            //}
                        }
                        else
                        {
                            pixel = glm::mix(pixel, r.value(), r.value().a * opacity);
                        }
                    }

                    _image->write(pixel, s, t, layer);
                }
            }
        }
    }
}

GeoImage::ReadResult
GeoImage::read(const GeoPoint& p, int layer) const
{
    if (!p.valid() || !valid())
    {
        return ResultFail;
    }

    // transform if necessary
    if (!p.srs.horizontallyEquivalentTo(srs()))
    {
        GeoPoint c = p.transform(srs());
        if (c.valid())
            return read(c);
        else
            return ResultFail;
        //return c.valid() && read(output, c);
    }

    double u = (p.x - _extent.xmin()) / _extent.width();
    double v = (p.y - _extent.ymin()) / _extent.height();

    // out of bounds?
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return ResultFail;
    }

    return _image->read_bilinear((float)u, (float)v, layer);
    //return true;
}

GeoImage::ReadResult
GeoImage::read(double x, double y, int layer) const
{
    if (!valid()) return ResultFail;

    double u = (x - _extent.xmin()) / _extent.width();
    double v = (y - _extent.ymin()) / _extent.height();

    // out of bounds?
    if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0)
    {
        return ResultFail;
    }

    return _image->read_bilinear((float)u, (float)v, layer);
}

GeoImage::ReadResult
GeoImage::read_clamped(double x, double y, int layer) const
{
    if (!valid()) return ResultFail;

    double u = (x - _extent.xmin()) / _extent.width();
    double v = (y - _extent.ymin()) / _extent.height();

    return _image->read_bilinear((float)clamp(u, 0.0, 1.0), (float)clamp(v, 0.0, 1.0), layer);
}

GeoImage::ReadResult
GeoImage::read(const SRS& xy_srs, double x, double y, int layer) const
{
    if (!valid())
        return ResultFail;

    if (!xy_srs.valid())
        return ResultFail;

    glm::dvec3 temp(x, y, 0);
    if (!srs().to(xy_srs).transform(temp, temp))
        return ResultFail;

    return read(temp.x, temp.y, layer);
}

GeoImage::ReadResult
GeoImage::read(const SRSOperation& xform, double x, double y, int layer) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), ResultFail);

    if (xform.noop())
        return read(x, y, layer);

    glm::dvec3 temp(x, y, 0);
    if (!xform.transform(temp, temp))
        return ResultFail;

    return read(temp.x, temp.y, layer);
}

void
GeoImage::computeMinMax()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(_image->valid() && _image->pixelFormat() == HEIGHTFIELD_FORMAT, void());

    _minValue = FLT_MAX;
    _maxValue = -FLT_MAX;

    Heightfield hf(_image);
    for (unsigned t = 0; t < hf.height(); ++t)
    {
        for (unsigned s = 0; s < hf.width(); ++s)
        {
            float height = hf.heightAt(s, t);
            if (height != hf.noDataValue())
            {
                _minValue = std::min(_minValue, height);
                _maxValue = std::max(_maxValue, height);
            }
        }
    }
}
