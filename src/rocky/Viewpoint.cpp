/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Viewpoint.h"

using namespace ROCKY_NAMESPACE;


Viewpoint::Viewpoint() :
    heading(Angle(0.0, Units::DEGREES)),
    pitch(Angle(-30.0, Units::DEGREES)),
    range(Distance(10000.0, Units::METERS))
{
    //nop
}

Viewpoint::Viewpoint(const Config& conf)
{
    conf.get( "name",    name );
    conf.get( "heading", heading );
    conf.get( "pitch",   pitch );
    conf.get( "range",   range );

    // piecewise point.
    std::string horiz = conf.value("srs");
    if ( horiz.empty() )
        horiz = "wgs84";

    const std::string vert = conf.value("vdatum");

    // try to parse an SRS, defaulting to WGS84 if not able to do so
    SRS srs(horiz, vert);

    // try x/y/z variant:
    if ( conf.hasValue("x") )
    {
        point = GeoPoint(
            srs,
            conf.value<double>("x", 0.0),
            conf.value<double>("y", 0.0),
            conf.value<double>("z", 0.0));
    }
    else if ( conf.hasValue("lat") )
    {
        point = GeoPoint(
            srs,
            conf.value<double>("long", 0.0),
            conf.value<double>("lat", 0.0),
            conf.value<double>("height", 0.0));
    }

    double xOffset = conf.value("x_offset", 0.0);
    double yOffset = conf.value("y_offset", 0.0);
    double zOffset = conf.value("z_offset", 0.0);
    if ( xOffset != 0.0 || yOffset != 0.0 || zOffset != 0.0 )
    {
        positionOffset = dvec3(xOffset, yOffset, zOffset);
    }
}

#define CONF_STR util::make_string() << std::fixed << std::setprecision(4)

Config
Viewpoint::getConfig() const
{
    Config conf( "viewpoint" );

    conf.set( "name",    name );
    conf.set( "heading", heading );
    conf.set( "pitch",   pitch );
    conf.set( "range",   range );
    
    if ( point.has_value() )
    {
        if ( point->srs().isGeographic() )
        {
            conf.set("long",   point->x());
            conf.set("lat",    point->y());
            conf.set("height", point->z());
        }
        else
        {
            conf.set("x", point->x());
            conf.set("y", point->y());
            conf.set("z", point->z());
        }

        conf.set("srs", point->srs().definition());

        if ( !point->srs().vertical().empty() )
            conf.set("vdatum", point->srs().vertical());
    }

    if (positionOffset.has_value() )
    {
        conf.set("x_offset", positionOffset->x);
        conf.set("y_offset", positionOffset->y);
        conf.set("z_offset", positionOffset->z);
    }

    return conf;
}

bool
Viewpoint::valid() const
{
    return
        (point.has_value() && point->valid());
}

std::string
Viewpoint::toString() const
{
    return util::make_string()
        << "x="   << point->x()
        << ", y=" << point->y()
        << ", z=" << point->z()
        << ", h=" << heading->to(Units::DEGREES).asParseableString()
        << ", p=" << pitch->to(Units::DEGREES).asParseableString()
        << ", d=" << range->asParseableString()
        << ", xo=" << positionOffset->x
        << ", yo=" << positionOffset->y
        << ", zo=" << positionOffset->z;
    //else
    //{
    //    return util::make_string()
    //        << "attached to node; "
    //        << ", h=" << _heading->to(Units::DEGREES).asParseableString()
    //        << ", p=" << _pitch->to(Units::DEGREES).asParseableString()
    //        << ", d=" << _range->asParseableString()
    //        << ", xo=" << _posOffset->x()
    //        << ", yo=" << _posOffset->y()
    //        << ", zo=" << _posOffset->z();
    //}
}
