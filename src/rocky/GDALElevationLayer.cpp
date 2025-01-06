/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GDALElevationLayer.h"
#ifdef ROCKY_HAS_GDAL

#include "Context.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::GDAL;

ROCKY_ADD_OBJECT_FACTORY(GDALElevation,
    [](const std::string& JSON, const IOOptions& io) {
        return GDALElevationLayer::create(JSON, io); })

namespace
{
    template<typename T>
    Status openOnThisThread(
        const T* layer,
        GDAL::Driver& driver,
        Profile* profile,
        DataExtentList* out_dataExtents,
        const IOOptions& io)
    {
        if (layer->maxDataLevel.has_value())
            driver.maxDataLevel = layer->maxDataLevel;

        if (layer->noDataValue.has_value())
            driver.noDataValue = layer->noDataValue;
        if (layer->minValidValue.has_value())
            driver.minValidValue = layer->minValidValue;
        if (layer->maxValidValue.has_value())
            driver.maxValidValue = layer->maxValidValue;

        Status status = driver.open(
            layer->name(),
            layer,
            layer->tileSize,
            out_dataExtents,
            io);

        if (status.failed())
            return status;

        if (driver.profile().valid() && profile != nullptr)
        {
            *profile = driver.profile();
        }

        return StatusOK;
    }
}


GDALElevationLayer::GDALElevationLayer() :
    super()
{
    construct({}, {});
}

GDALElevationLayer::GDALElevationLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
GDALElevationLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("GDALElevation");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "connection", connection);
    get_to(j, "subdataset", subDataset);
    std::string temp;
    get_to(j, "interpolation", temp);
    if (temp == "nearest") interpolation = Interpolation::NEAREST;
    else if (temp == "bilinear") interpolation = Interpolation::BILINEAR;
    get_to(j, "single_threaded", singleThreaded);

    // default for GDAL elevation is nearest-neighbor.
    if (!interpolation.has_value())
    {
        interpolation.set_default(Interpolation::NEAREST);
    }

    setRenderType(RenderType::TERRAIN_SURFACE);
}

JSON
GDALElevationLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", uri);
    set(j, "connection", connection);
    set(j, "subdataset", subDataset);
    if (interpolation.has_value(Interpolation::NEAREST))
        set(j, "interpolation", "nearest");
    else if (interpolation.has_value(Interpolation::BILINEAR))
        set(j, "interpolation", "bilinear");
    set(j, "single_threaded", singleThreaded);
    return j.dump();
}

Status
GDALElevationLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile new_profile;

    // GDAL thread-safety requirement: each thread requires a separate GDALDataSet.
    // So we just encapsulate the entire setup once per thread.
    // https://trac.osgeo.org/gdal/wiki/FAQMiscellaneous#IstheGDALlibrarythread-safe

    auto& driver = _drivers.value();

    DataExtentList dataExtents;

    Status s = openOnThisThread(this, driver, &new_profile, &dataExtents, io);

    if (s.failed())
        return s;

    // if the driver generated a valid profile, set it.
    if (new_profile.valid())
    {
        profile = new_profile;
    }

    setDataExtents(dataExtents);

    return s;
}

void
GDALElevationLayer::closeImplementation()
{
    // safely shut down all per-thread handles.
    _drivers.clear();

    super::closeImplementation();
}

Result<GeoHeightfield>
GDALElevationLayer::createHeightfieldImplementation(const TileKey& key, const IOOptions& io) const
{
    if (status().failed())
        return status();

    auto& driver = _drivers.value();
    if (!driver.isOpen())
    {
        // calling openImpl with NULL params limits the setup
        // since we already called this during openImplementation
        openOnThisThread(this, driver, nullptr, nullptr, io);
    }

    if (driver.isOpen())
    {
        auto r = driver.createHeightfield(key, tileSize, io);

        if (r.status.ok())
        {
            return GeoHeightfield(r.value, key.extent());
        }
        else
        {
            auto r = driver.createImage(key, tileSize, io);

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
        }
    }

    return Status_ResourceUnavailable;
}

#endif // GDAL_HAS_ROCKY