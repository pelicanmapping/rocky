#include "GeoHeightfield.h"
#include "Heightfield.h"
#include "Math.h"

using namespace ROCKY_NAMESPACE;

#define LC "[GeoHeightfield] "

// static
GeoHeightfield GeoHeightfield::INVALID;

GeoHeightfield::GeoHeightfield() :
    _extent(GeoExtent::INVALID),
    _minHeight(0.0f),
    _maxHeight(0.0f)
{
    init();
}

GeoHeightfield&
GeoHeightfield::operator=(GeoHeightfield&& rhs)
{
    *this = (const GeoHeightfield&)rhs;
    rhs._extent = GeoExtent::INVALID;
    return *this;
}

GeoHeightfield::GeoHeightfield(
    shared_ptr<Heightfield> heightField,
    const GeoExtent&  extent) :

    _hf(heightField),
    _extent(extent),
    _minHeight(FLT_MAX),
    _maxHeight(-FLT_MAX)
{
    init();
}

bool
GeoHeightfield::valid() const
{
    return _hf && _extent.valid();
}

void
GeoHeightfield::init()
{
    if ( _hf )
    {
        _resolution.x = _extent.width() / (double)(_hf->width() - 1);
        _resolution.y = _extent.height() / (double)(_hf->height() - 1);

        for (unsigned row = 0; row < _hf->height(); ++row)
        {
            for (unsigned col = 0; col < _hf->width(); ++col)
            {
                float h = _hf->heightAt(col, row);
                _maxHeight = std::max(_maxHeight, h);
                _minHeight = std::min(_minHeight, h);
            }
        }
    }
}

float
GeoHeightfield::heightAtLocation(
    double x,
    double y,
    Image::Interpolation interpolation) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(valid(), NO_DATA_VALUE);

    double px = clamp((x - _extent.xmin()) / _resolution.x, 0.0, (double)(_hf->width() - 1));
    double py = clamp((y - _extent.ymin()) / _resolution.y, 0.0, (double)(_hf->height() - 1));

    return _hf->heightAtPixel(px, py, interpolation);
}

float
GeoHeightfield::heightAt(double x, double y, const SRSOperation& xform, Image::Interpolation interp) const
{
    dvec3 temp(x, y, 0);
    if (xform.valid())
    {
        if (!xform.transform(temp, temp))
            return NO_DATA_VALUE;
    }

    // check that the point falls within the heightfield bounds:
    if (_extent.contains(temp.x, temp.y))
    {
        // sample the heightfield at the input coordinates:
        temp.z = heightAtLocation(temp.x, temp.y, interp);

        if (xform.valid())
        {
            xform.inverse(temp, temp);
        }

        return temp.z;
    }
    else
    {
        return NO_DATA_VALUE;
    }
}

float
GeoHeightfield::heightAt(double x, double y, const SRS& xy_srs, Image::Interpolation interp) const
{
    const SRS& localSRS = _extent.srs();

    dvec3 temp(x, y, 0);

    // first xform the input point into our local SRS:
    SRSOperation xform;
    if (xy_srs != localSRS)
    {
        xform = xy_srs.to(localSRS);
        if (xform.valid())
        {
            if (!xform(temp, temp))
                return NO_DATA_VALUE;
        }
    }

    // check that the point falls within the heightfield bounds:
    if (_extent.contains(temp.x, temp.y))
    {
        // sample the heightfield at the input coordinates:
        temp.z = heightAtLocation(temp.x, temp.y, interp);

        if (xform.valid())
            xform.inverse(temp, temp);

        return temp.z;
    }
    else
    {
        return NO_DATA_VALUE;
    }
}

GeoHeightfield
GeoHeightfield::createSubSample(
    const GeoExtent& destEx,
    unsigned width,
    unsigned height,
    Image::Interpolation interpolation) const
{
    double div = destEx.width()/_extent.width();
    if ( div >= 1.0f )
        return GeoHeightfield::INVALID;

    double dx = destEx.width()/(double)(width-1);
    double dy = destEx.height()/(double)(height-1);    

    shared_ptr<Heightfield> dest = Heightfield::create(width, height);

    double x, y;
    int col, row;

    double x0 = (destEx.xmin()-_extent.xmin())/_extent.width();
    double y0 = (destEx.ymin()-_extent.ymin())/_extent.height();

    double xstep = div / (double)(width-1);
    double ystep = div / (double)(height-1);
    
    for( x = x0, col = 0; col < (int)width; x += xstep, col++ )
    {
        for( y = y0, row = 0; row < (int)height; y += ystep, row++ )
        {
            float heightAtNL = _hf->heightAtUV(x, y, interpolation );
            dest->data<float>(col, row) = heightAtNL;
        }
    }

    return GeoHeightfield( dest, destEx ); // Q: is the VDATUM accounted for?
}

const GeoExtent&
GeoHeightfield::extent() const
{
    return _extent;
}
