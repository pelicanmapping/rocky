/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GDALImageLayer.h"
#include "Image.h"
#include "Log.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::GDAL;

namespace
{
    template<typename T>
    Status openOnThisThread(
        const T* layer,
        shared_ptr<GDAL::Driver>& driver,
        Profile* profile,
        DataExtentList* out_dataExtents,
        const IOOptions& io)
    {
        driver = std::make_shared<GDAL::Driver>();

        if (layer->maxDataLevel().has_value())
            driver->setMaxDataLevel(layer->maxDataLevel());

        Status status = driver->open(
            layer->name(),
            layer,
            layer->tileSize(),
            out_dataExtents,
            io);

        if (status.failed())
            return status;

        if (driver->profile().valid() && profile != nullptr)
        {
            *profile = driver->profile();
        }

        return StatusOK;
    }
}


GDALImageLayer::GDALImageLayer() :
    super()
{
    construct({});
}

GDALImageLayer::GDALImageLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
GDALImageLayer::construct(const JSON& conf)
{
    setConfigKey("GDALImage");
    const auto j = parse_json(conf);
    get_to(j, "uri", _uri);
    get_to(j, "connection", _connection);
    get_to(j, "subdataset", _subDataSet);
    std::string temp;
    get_to(j, "interpolation", temp);
    if (temp == "nearest") _interpolation = Image::NEAREST;
    else if (temp == "bilinear") _interpolation = Image::BILINEAR;
    get_to(j, "single_threaded", _singleThreaded);

    setRenderType(RENDERTYPE_TERRAIN_SURFACE);
}

JSON
GDALImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "uri", _uri);
    set(j, "connection", _connection);
    set(j, "subdataset", _subDataSet);
    if (_interpolation.has_value(Image::NEAREST))
        set(j, "interpolation", "nearest");
    else if (_interpolation.has_value(Image::BILINEAR))
        set(j, "interpolation", "bilinear");
    set(j, "single_threaded", _singleThreaded);
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

    Status s = openOnThisThread(
        this,
        driver,
        &profile,
        &dataExtents,
        io);

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
GDALImageLayer::createImageImplementation(
    const TileKey& key,
    const IOOptions& io) const
{
    ROCKY_PROFILE_FUNCTION();

    if (status().failed())
        return status();

    auto& driver = _drivers.value();
    if (driver == nullptr)
    {
        // calling openImpl with NULL params limits the setup
        // since we already called this during openImplementation
        openOnThisThread(this, driver, nullptr, nullptr, io);
    }

    if (driver)
    {
        auto image = driver->createImage(
            key,
            _tileSize,
            coverage() == true,
            io);

        return GeoImage(image.value, key.extent());
    }

    return GeoImage::INVALID;
}
