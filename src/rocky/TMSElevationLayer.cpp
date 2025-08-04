/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSElevationLayer.h"
#include "Context.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

ROCKY_ADD_OBJECT_FACTORY(TMSElevation, 
    [](const std::string& JSON, const IOOptions& io) {
        return TMSElevationLayer::create(JSON, io); })

TMSElevationLayer::TMSElevationLayer() :
    super()
{
    construct({}, {});
}

TMSElevationLayer::TMSElevationLayer(std::string_view JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
TMSElevationLayer::construct(std::string_view JSON, const IOOptions& io)
{
    setLayerTypeName("TMSElevation");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "format", format);
    get_to(j, "invert_y", invertY);
}

std::string
TMSElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", uri);
    set(j, "format", format);
    set(j, "invert_y", invertY);
    return j.dump();
}

Result<>
TMSElevationLayer::openImplementation(const IOOptions& io)
{
    auto parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile driver_profile = profile;

    DataExtentList dataExtents;
    auto r = _driver.open(
        uri,
        driver_profile,
        format,
        dataExtents,
        io);

    if (r.failed())
        return r;

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
TMSElevationLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoHeightfield>
TMSElevationLayer::createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const
{
    if (status().failed())
        return status().error();

    // request
    auto r = _driver.read(key, invertY, encoding == Encoding::MapboxRGB, uri->context(), io);

    if (r.ok())
    {
        if (r.value()->pixelFormat() == Image::R32_SFLOAT)
        {
            return GeoHeightfield(Heightfield::create(r.value().get()), key.extent());
        }
        else // assume Image::R8G8B8_UNORM?
        {
            auto hf = decodeRGB(r.value());
            return GeoHeightfield(hf, key.extent());
        }
    }
    else
    {
        if (r.error().type == Failure::ServiceUnavailable)
        {
            fail(r.error());
            Log()->warn(LC "Layer \"" + name + "\" : " + r.error().message);
        }

        return r.error();
    }
}
