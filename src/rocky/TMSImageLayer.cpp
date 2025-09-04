/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSImageLayer.h"
#include "Context.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

ROCKY_ADD_OBJECT_FACTORY(TMSImage,
    [](std::string_view JSON, const IOOptions& io) {
        return TMSImageLayer::create(JSON, io); })


ROCKY_ADD_OBJECT_FACTORY(XYZImage,
    [](std::string_view JSON, const IOOptions& io) {
        auto layer = TMSImageLayer::create(JSON, io);
        if (!layer->profile.valid())
            layer->profile = Profile("spherical-mercator");
        return layer;
    })

TMSImageLayer::TMSImageLayer() :
    super()
{
    construct({}, {});
}

TMSImageLayer::TMSImageLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
TMSImageLayer::construct(std::string_view JSON, const IOOptions& io)
{
    setLayerTypeName("TMSImage");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "format", format);
    get_to(j, "invert_y", invertY);
}

std::string
TMSImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());   
    set(j, "uri", uri);
    set(j, "format", format);
    set(j, "invert_y", invertY);
    return j.dump();
}

Result<>
TMSImageLayer::openImplementation(const IOOptions& io)
{
    auto parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile driver_profile = profile;

    DataExtentList dataExtents;
    auto status = _driver.open(uri, driver_profile, format, dataExtents, io);

    if (status.failed())
        return status.error();

    if (driver_profile != profile)
    {
        profile = driver_profile;
    }

    // If the layer name is unset, try to set it from the tileMap title.
    if (name.empty() && !_driver.tileMap.title.empty())
    {
        name = _driver.tileMap.title;
    }

    setDataExtents(dataExtents);

    return ResultVoidOK;
}

void
TMSImageLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoImage>
TMSImageLayer::createTileImplementation(const TileKey& key, const IOOptions& io) const
{
    auto r = _driver.read(key, invertY, false, uri->context(), io);

    if (r.ok())
    {
        return GeoImage(r.value(), key.extent());
    }
    else
    {
        return r.error();
    }
}


const GeoExtent&
TMSImageLayer::extent() const
{
    if (crop.has_value())
    {
        return crop.value();
    }
    else if (_driver.tileMapExtent.valid())
    {
        return _driver.tileMapExtent;
    }
    else
    {
        return super::extent();
    }
}