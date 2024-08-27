/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MBTilesImageLayer.h"

#ifdef ROCKY_HAS_MBTILES

#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

ROCKY_ADD_OBJECT_FACTORY(MBTilesImage,
    [](const std::string& JSON, const IOOptions& io) {
        return MBTilesImageLayer::create(JSON, io); })


MBTilesImageLayer::MBTilesImageLayer() :
    super()
{
    construct({}, {});
}

MBTilesImageLayer::MBTilesImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
MBTilesImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("MBTilesImage");
    const auto j = parse_json(JSON);
    get_to(j, "uri", _options.uri, io);
    get_to(j, "format", _options.format);
    get_to(j, "compress", _options.compress);
}

JSON
MBTilesImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", _options.uri);
    set(j, "format", _options.format);
    set(j, "compress", _options.compress);
    return j.dump();
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
        false, // isWritingRequested(),
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

#endif // ROCKY_HAS_MBTILES
