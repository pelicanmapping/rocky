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

        //if (_heightField->getOrigin() != origin ||
        //    _heightField->getXInterval() != dx ||
        //    _heightField->getYInterval() != dy)
        //{
        //    osg::HeightField* hf = new osg::HeightField(*_heightField.get(), osg::CopyOp::SHALLOW_COPY);
        //    hf->setOrigin(origin);
        //    hf->setXInterval(dx);
        //    hf->setYInterval(dy);
        //    hf->setBorderWidth(0);
        //    _heightField = hf;
        //}

        auto size = _hf->width() * _hf->height();
        for (auto i = 0u; i < size; ++i)
        {
            float h = _hf->data<float>(i);
            _maxHeight = std::max(_maxHeight, h);
            _minHeight = std::min(_minHeight, h);
        }
        //const osg::HeightField::HeightList& heights = _heightField->getHeightList();
        //for( unsigned i=0; i<heights.size(); ++i )
        //{
        //    float h = heights[i];
        //    if ( h > _maxHeight ) _maxHeight = h;
        //    if ( h < _minHeight ) _minHeight = h;
        //}
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

bool
GeoHeightfield::getElevation(
    const SRS& inputSRS,
    dvec3& in_out_point,
    Image::Interpolation interp) const
{
    const SRS& localSRS = _extent.srs();

    dvec3 local;

    // first xform the input point into our local SRS:
    SRSTransform xform;
    if (inputSRS != localSRS)
    {
        xform = inputSRS.to(localSRS);
    }

    if (xform.valid())
    {
        if (!xform(in_out_point, local))
            return false;
    }
    else
    {
        local = in_out_point;
    }

    // check that the point falls within the heightfield bounds:
    if (_extent.contains(local.x, local.y))
    {
        // sample the heightfield at the input coordinates:
        local.z = heightAtLocation(local.x, local.y, interp);

        if (xform.valid())
            xform.inverse(local, in_out_point);
        else
            in_out_point = local;

        if (local.z == NO_DATA_VALUE)
            in_out_point.z = NO_DATA_VALUE;

        return true;
    }
    else
    {
        in_out_point.z = 0.0;
        return false;
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
