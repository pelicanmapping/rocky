/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSImageLayer.h"
#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

ROCKY_ADD_OBJECT_FACTORY(TMSImage,
    [](const JSON& conf) { return TMSImageLayer::create(conf); })

TMSImageLayer::TMSImageLayer() :
    super()
{
    construct(JSON());
}

TMSImageLayer::TMSImageLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
TMSImageLayer::construct(const JSON& conf)
{
    setConfigKey("TMSImage");
    const auto j = parse_json(conf);
    get_to(j, "uri", uri);
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
        setProfileDefault(driver_profile);
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
    ROCKY_PROFILE_FUNCTION();

    auto r = _driver.read(uri, key, invertY, false, io);

    if (r.status.ok())
        return GeoImage(r.value, key.extent());
    else
        return r.status;
}

#if 0
Status
TMSImageLayer::writeImageImplementation(const TileKey& key, const osg::Image* image, ProgressCallback* progress) const
{
    if (!isWritingRequested())
        return Status::ServiceUnavailable;

    bool ok = _driver.write(
        options().url().get(),
        key,
        image,
        options().tmsType().get() == "google",
        progress,
        getReadOptions());

    if (!ok)
    {
        return Status::ServiceUnavailable;
    }

    return STATUS_OK;
}
#endif
