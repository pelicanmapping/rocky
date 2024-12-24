/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GDALImageLayer.h"
#ifdef ROCKY_HAS_GDAL

#include "Context.h"
#include "Image.h"
#include "Log.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::GDAL;

ROCKY_ADD_OBJECT_FACTORY(GDALImage,
    [](const std::string& JSON, const IOOptions& io) {
        return GDALImageLayer::create(JSON, io); })

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
        if (layer->maxDataLevel().has_value())
        {
            driver.maxDataLevel = layer->maxDataLevel();
        }

        Status status = driver.open(
            layer->name(),
            layer,
            layer->tileSize(),
            out_dataExtents,
            io);

        if (status.failed())
            return status;

        //ROCKY_HARD_ASSERT(driver != nullptr);

        if (driver.profile().valid() && profile != nullptr)
        {
            *profile = driver.profile();
        }

        return StatusOK;
    }
}


GDALImageLayer::GDALImageLayer() :
    super()
{
    construct({}, {});
}

GDALImageLayer::GDALImageLayer(const std::string& JSON, const IOOptions& io) :
    super(JSON, io)
{
    construct(JSON, io);
}

void
GDALImageLayer::construct(const std::string& JSON, const IOOptions& io)
{
    setLayerTypeName("GDALImage");
    const auto j = parse_json(JSON);
    get_to(j, "uri", uri, io);
    get_to(j, "connection", connection);
    get_to(j, "subdataset", subDataset);
    std::string temp;
    get_to(j, "interpolation", temp);
    if (temp == "nearest") interpolation = Image::NEAREST;
    else if (temp == "bilinear") interpolation = Image::BILINEAR;
    get_to(j, "single_threaded", singleThreaded);

    setRenderType(RenderType::TERRAIN_SURFACE);
}

JSON
GDALImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", uri);
    set(j, "connection", connection);
    set(j, "subdataset", subDataset);
    if (interpolation.has_value(Image::NEAREST))
        set(j, "interpolation", "nearest");
    else if (interpolation.has_value(Image::BILINEAR))
        set(j, "interpolation", "bilinear");
    set(j, "single_threaded", singleThreaded);
    return j.dump();
}

Status
GDALImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    Profile profile;

    // GDAL thread-safety requirement: each thread requires a separate GDALDataSet.
    // So we just encapsulate the entire setup once per thread.
    // https://trac.osgeo.org/gdal/wiki/FAQMiscellaneous#IstheGDALlibrarythread-safe

    auto& driver = _drivers.value();

    DataExtentList dataExtents;

    Status s = openOnThisThread(this, driver, &profile, &dataExtents, io);

    if (s.failed())
        return s;

    // if the driver generated a valid profile, set it.
    if (profile.valid())
    {
        setProfile(profile);
    }

    setDataExtents(dataExtents);

    return s;
}

void
GDALImageLayer::closeImplementation()
{
    // safely shut down all per-thread handles.
    _drivers.clear();

    super::closeImplementation();
}

Result<GeoImage>
GDALImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
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
        auto image = driver.createImage(key, _tileSize, io);
        if (image.value)
            return GeoImage(image.value, key.extent());
    }

    return Status_ResourceUnavailable;
}

#endif // ROCKY_HAS_GDAL
