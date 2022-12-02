#include "GeoHeightfield.h"
#include "Heightfield.h"
#include "VerticalDatum.h"
#include "Math.h"

using namespace rocky;

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
GeoHeightfield::getHeightAtLocation(
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
    shared_ptr<SRS> inputSRS,
    double x,
    double y,
    Image::Interpolation interp,
    shared_ptr<SRS> outputSRS,
    float& out_elevation) const
{
    dvec3 xy(x, y, 0);
    dvec3 local = xy;
    shared_ptr<SRS> extentSRS = _extent.getSRS();

    // first xform the input point into our local SRS:
    if (inputSRS != extentSRS)
    {
        if (inputSRS && !inputSRS->transform(xy, extentSRS, local))
            return false;
    }

    // check that the point falls within the heightfield bounds:
    if (_extent.contains(local.x, local.y))
    {
        //double xInterval = _extent.width()  / (double)(_hf->getNumColumns()-1);
        //double yInterval = _extent.height() / (double)(_heightField->getNumRows()-1);

        // sample the heightfield at the input coordinates:
        // (note: since it's sampling the HF, it will return an MSL height if applicable)
        out_elevation = getHeightAtLocation(
            local.x, local.y, interp);
        //_heightField.get(), 
        //local.x(), local.y(),
            //_extent.xMin(), _extent.yMin(), 
            //xInterval, yInterval, 
            //interp);

        // if the vertical datums don't match, do a conversion:
        if (out_elevation != NO_DATA_VALUE && 
            outputSRS &&
            !extentSRS->isVertEquivalentTo(outputSRS.get()))
        {
            // if the caller provided a custom output SRS, perform the appropriate
            // Z transformation. This requires a lat/long point:

            dvec3 geolocal(local);
            if (!extentSRS->isGeographic())
            {
                extentSRS->transform(geolocal, extentSRS->getGeographicSRS(), geolocal);
            }

            VerticalDatum::transform(
                extentSRS->getVerticalDatum().get(),
                outputSRS->getVerticalDatum().get(),
                geolocal.y, geolocal.x,
                out_elevation);
        }

        return true;
    }
    else
    {
        out_elevation = 0.0f;
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
GeoHeightfield::getExtent() const
{
    return _extent;
}
