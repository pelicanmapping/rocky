/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MBTilesImageLayer.h"

using namespace ROCKY_NAMESPACE;

MBTilesImageLayer::MBTilesImageLayer() :
    super()
{
    construct(Config());
}

MBTilesImageLayer::MBTilesImageLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
MBTilesImageLayer::construct(const Config& conf)
{
    setConfigKey("MBTilesImage");
    conf.get("uri", _options.uri);
    conf.get("format", _options.format);
    conf.get("compress", _options.compress);
}

Config
MBTilesImageLayer::getConfig() const
{
    Config conf = super::getConfig();
    conf.set("uri", _options.uri);
    conf.set("format", _options.format);
    conf.set("compress", _options.compress);
    return conf;
}

Status
MBTilesImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile new_profile = profile();
    DataExtentList dataExtents;

    Status status = _driver.open(
        name(),
        _options,
        isWritingRequested(),
        new_profile,
        dataExtents,
        io);

    if (status.failed())
    {
        return status;
    }

    // install the profile if there is one
    if (!profile().valid() && new_profile.valid())
    {
        setProfile(new_profile);
    }

    setDataExtents(dataExtents);

    return StatusOK;
}

void
MBTilesImageLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoImage>
MBTilesImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
{
    if (status().failed()) return status();

    auto result = _driver.read(key, io);

    if (result.status.ok())
        return GeoImage(result.value, key.extent());
    else
        return result.status;
}

Status
MBTilesImageLayer::writeImageImplementation(const TileKey& key, shared_ptr<Image> image, const IOOptions& io) const
{
    if (status().failed()) return status();

    if (!isWritingRequested())
        return Status(Status::ServiceUnavailable);

    return _driver.write(key, image, io);
}
