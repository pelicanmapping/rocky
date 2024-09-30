/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSImageLayer.h"
#ifdef ROCKY_HAS_TMS

#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

ROCKY_ADD_OBJECT_FACTORY(TMSImage,
    [](const std::string& JSON, const IOOptions& io) {
        return TMSImageLayer::create(JSON, io); })


ROCKY_ADD_OBJECT_FACTORY(XYZImage,
    [](const std::string& JSON, const IOOptions& io) {
        return TMSImageLayer::create(JSON, io); })

TMSImageLayer::TMSImageLayer() :
    super()
{
    construct({}, {});
}

TMSImageLayer::TMSImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
TMSImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("TMSImage");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "format", format);
    get_to(j, "invert_y", invertY);
}

JSON
TMSImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());   
    set(j, "uri", uri);
    set(j, "format", format);
    set(j, "invert_y", invertY);
    return j.dump();
}

Status
TMSImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile driver_profile = profile();

    DataExtentList dataExtents;
    Status status = _driver.open(uri, driver_profile, format, dataExtents, io);

    if (status.failed())
        return status;

    if (driver_profile != profile())
    {
        setProfile(driver_profile);
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
TMSImageLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoImage>
TMSImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
{
    auto r = _driver.read(key, invertY, false, uri->context(), io);

    if (r.status.ok())
        return GeoImage(r.value, key.extent());
    else
        return r.status;
}


#endif // ROCKY_HAS_TMS
