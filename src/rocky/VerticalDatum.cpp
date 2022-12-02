/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "VerticalDatum.h"
#include "Threading.h"
#include "Geoid.h"
//#include "GeoHeightfield.h"
#include "Heightfield.h"
#include <cstdlib>

using namespace rocky;
using namespace rocky::util;

#undef LC
#define LC "[VerticalDatum] "

// --------------------------------------------------------------------------

#include "EGM96Grid.cpp"

namespace
{
    using VDatumCache = std::unordered_map<std::string, shared_ptr<VerticalDatum>>;
    VDatumCache _vdatumCache;
    util::Mutex _vdataCacheMutex("VDatumCache(OE)");
    bool _vdatumWarning = false;
} 

shared_ptr<VerticalDatum>
VerticalDatum::get(const std::string& initString)
{
    shared_ptr<VerticalDatum> result;

    if (initString.empty())
        return result;

    util::ScopedMutexLock exclusive(_vdataCacheMutex);

    if (::getenv("ROCKY_IGNORE_VERTICAL_DATUMS"))
    {
        if (!_vdatumWarning)
        {
            ROCKY_WARN << LC << "WARNING *** Vertical datums have been deactivated; elevation values may be wrong!" << std::endl;
            _vdatumWarning = true;
        }
        return nullptr;
    }

    // check the cache:
    std::string s = util::toLower(initString);
    auto i = _vdatumCache.find(s);
    if (i != _vdatumCache.end())
    {
        result = i->second;
    }

    if (!result)
    {
        ROCKY_DEBUG << LC << "Initializing vertical datum: " << initString << std::endl;

        if (s == "egm96")
        {
            result = VerticalDatum::create("egm96", EGM96::create());
        }

        if (result)
        {
            _vdatumCache[s] = result;
        }
    }

    return result;
}

// --------------------------------------------------------------------------

VerticalDatum::VerticalDatum(
    const std::string& name,
    shared_ptr<Geoid> geoid) :

    _name(name),
    _geoid(geoid)
{
    //nop
}

bool
VerticalDatum::transform(
    const VerticalDatum* from,
    const VerticalDatum* to,
    double lat_deg,
    double lon_deg,
    double& in_out_z)
{
    if ( from == to )
        return true;

    if ( from )
    {
        in_out_z = from->msl2hae( lat_deg, lon_deg, in_out_z );
    }

    Units fromUnits = from ? from->getGeoid()->units : Units::METERS;
    Units toUnits = to ? to->getGeoid()->units : Units::METERS;

    in_out_z = fromUnits.convertTo(toUnits, in_out_z);

    if ( to )
    {
        in_out_z = to->hae2msl( lat_deg, lon_deg, in_out_z );
    }

    return true;
}

bool
VerticalDatum::transform(
    const VerticalDatum* from,
    const VerticalDatum* to,
    double lat_deg,
    double lon_deg,
    float& in_out_z)
{
    double d(in_out_z);
    bool ok = transform(from, to, lat_deg, lon_deg, d);
    if (ok) in_out_z = float(d);
    return ok;
}

bool
VerticalDatum::transform(
    const VerticalDatum* from,
    const VerticalDatum* to,
    const GeoExtent& extent,
    shared_ptr<Heightfield> hf)
{
    if (from == to)
        return true;

    unsigned cols = hf->width();
    unsigned rows = hf->height();
    
    dvec3 sw(extent.west(), extent.south(), 0.0);
    dvec3 ne(extent.east(), extent.north(), 0.0);
    
    double xstep = std::abs(extent.east() - extent.west()) / double(cols-1);
    double ystep = std::abs(extent.north() - extent.south()) / double(rows-1);

    if ( !extent.getSRS()->isGeographic() )
    {
        auto geoSRS = extent.getSRS()->getGeographicSRS();
        extent.getSRS()->transform(sw, geoSRS, sw);
        extent.getSRS()->transform(ne, geoSRS, ne);
        xstep = (ne.x-sw.x) / double(cols-1);
        ystep = (ne.y-sw.y) / double(rows-1);
    }

    for( unsigned c=0; c<cols; ++c)
    {
        double lon = sw.x + xstep*double(c);
        for( unsigned r=0; r<rows; ++r)
        {
            double lat = sw.y + ystep*double(r);
            float& h = hf->data<float>(c, r); // getHeight(c, r);
            if (h != NO_DATA_VALUE)
            {
                VerticalDatum::transform( from, to, lat, lon, h );
            }
        }
    }

    return true;
}

double 
VerticalDatum::msl2hae( double lat_deg, double lon_deg, double msl ) const
{
    //GeoHeightfield temp(_geoid, _extent);
    //return _geoid ? msl + temp.getHeightAtLocation(lat_deg, lon_deg, BILINEAR) : msl;
    return _geoid ? msl + _geoid->getHeight(lat_deg, lon_deg, Image::BILINEAR) : msl;
}

double
VerticalDatum::hae2msl( double lat_deg, double lon_deg, double hae ) const
{
    //GeoHeightfield temp(_geoid, _extent);
    //return _geoid ? hae - temp.getHeightAtLocation(lat_deg, lon_deg, BILINEAR) : hae;
    return _geoid ? hae - _geoid->getHeight(lat_deg, lon_deg, Image::BILINEAR) : hae;
}

bool
VerticalDatum::isEquivalentTo(const VerticalDatum* rhs) const
{
    if (this == rhs)
        return true;

    if (rhs == nullptr)
        return !_geoid;

    if (_geoid != rhs->_geoid)
        return false;

    if (_geoid && rhs->_geoid && _geoid->name != rhs->_geoid->name)
        return false;

    return true;
}

//
////------------------------------------------------------------------------
//
//#undef  LC
//#define LC "[VerticalDatumFactory] "
//
//shared_ptr<VerticalDatum>
//VerticalDatumFactory::create(const std::string& init)
//{
//    shared_ptr<VerticalDatum> datum;
//
//    std::string driverExt = Stringify() << ".osgearth_vdatum_" << init;
//    osg::ref_ptr<osg::Object> object = osgDB::readRefObjectFile( driverExt );
//    datum = dynamic_cast<VerticalDatum*>( object.release() );
//    if ( !datum )
//    {
//        ROCKY_WARN << "WARNING: Failed to load Vertical Datum driver for \"" << init << "\"" << std::endl;
//    }
//
//    return datum.release();
//}
