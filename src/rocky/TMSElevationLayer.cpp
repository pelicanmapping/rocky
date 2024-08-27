/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSElevationLayer.h"
#ifdef ROCKY_HAS_TMS

#include "Instance.h"
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

TMSElevationLayer::TMSElevationLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
TMSElevationLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("TMSElevation");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "format", format);
    get_to(j, "invert_y", invertY);
}

JSON
TMSElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", uri);
    set(j, "format", format);
    set(j, "invert_y", invertY);
    return j.dump();
}

Status
TMSElevationLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile driver_profile = profile();

    DataExtentList dataExtents;
    Status status = _driver.open(
        uri,
        driver_profile,
        format,
        dataExtents,
        io);

    if (status.failed())
        return status;

    if (driver_profile != profile())
    {
        setProfileAsDefault(driver_profile);
    }

    // If the layer name is unset, try to set it from the tileMap title.
    if (name().empty() && !_driver.tileMap.title.empty())
    {
        setName(_driver.tileMap.title);
    }

    setDataExtents(dataExtents);

    return StatusOK;
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
    if (!isOpen())
        return status();

    // request
    auto r = _driver.read(uri, key, invertY, _encoding == Encoding::MapboxRGB, io);

    if (r.status.ok())
    {
        if (r.value->pixelFormat() == Image::R32_SFLOAT)
        {
            return GeoHeightfield(Heightfield::create(r.value.get()), key.extent());
        }
        else // assume Image::R8G8B8_UNORM?
        {
            auto hf = decodeMapboxRGB(r.value);
            return GeoHeightfield(hf, key.extent());
        }
    }
    else
    {
        if (r.status.code == Status::ServiceUnavailable)
        {
            setStatus(r.status);
            Log()->warn(LC "Layer \"" + name() + "\" : " + r.status.message);
        }

        return r.status;
    }
}

#endif // ROCKY_HAS_TMS
