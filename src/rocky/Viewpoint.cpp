/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Viewpoint.h"

using namespace ROCKY_NAMESPACE;

Viewpoint::Viewpoint(const Config& conf)
{
    conf.get("name", name);
    conf.get("heading", heading);
    conf.get("pitch", pitch);
    conf.get("range", range);

    if (conf.hasChild("target"))
    {
        GeoPoint point(conf.child("target"));
        if (point.valid())
        {
            target = std::make_shared<SimpleMapObject>(std::move(point));
        }
    }

    double xOffset = conf.value("x_offset", 0.0);
    double yOffset = conf.value("y_offset", 0.0);
    double zOffset = conf.value("z_offset", 0.0);
    if (xOffset != 0.0 || yOffset != 0.0 || zOffset != 0.0)
    {
        positionOffset = dvec3(xOffset, yOffset, zOffset);
    }
}

Config
Viewpoint::getConfig() const
{
    Config conf( "viewpoint" );

    conf.set("name", name);
    conf.set("heading", heading);
    conf.set("pitch", pitch);
    conf.set("range", range);

    if (target && target->position().valid())
    {
        conf.add("target", target->getConfig());
    }

    return conf;
}

std::string
Viewpoint::toString() const
{
    return util::make_string()
        << "x="   << (valid()? std::to_string(target->position().x()) : "?")
        << ", y=" << (valid() ? std::to_string(target->position().y()) : "?")
        << ", z=" << (valid() ? std::to_string(target->position().z()) : "?")
        << ", h=" << heading->to(Units::DEGREES).asParseableString()
        << ", p=" << pitch->to(Units::DEGREES).asParseableString()
        << ", d=" << range->asParseableString()
        << ", xo=" << positionOffset->x
        << ", yo=" << positionOffset->y
        << ", zo=" << positionOffset->z;
}
