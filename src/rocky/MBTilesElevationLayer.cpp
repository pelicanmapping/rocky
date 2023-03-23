/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MBTilesElevationLayer.h"

using namespace ROCKY_NAMESPACE;

MBTilesElevationLayer::MBTilesElevationLayer()
{
    //nop
}

Status
MBTilesElevationLayer::openImplementation(const IOOptions& io)
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
MBTilesElevationLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoHeightfield>
MBTilesElevationLayer::createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const
{
    if (status().failed()) return status();

    auto result = _driver.read(key, io);

    if (result.status.ok())
        return GeoHeightfield(Heightfield::create(result.value.get()), key.extent());
    else
        return result.status;
}

Status
MBTilesElevationLayer::writeHeightfieldImplementation(const TileKey& key, shared_ptr<Heightfield> image, const IOOptions& io) const
{
    if (status().failed()) return status();

    if (!isWritingRequested())
        return Status(Status::ServiceUnavailable);

    return _driver.write(key, image, io);
}
