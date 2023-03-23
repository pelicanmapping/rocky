/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GDALImageLayer.h"
#include "Image.h"
#include "Log.h"

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
    construct(Config());
}

GDALImageLayer::GDALImageLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
GDALImageLayer::construct(const Config& conf)
{
    setConfigKey("GDALImage");

    conf.get("url", _uri);
    conf.get("uri", _uri);
    conf.get("connection", _connection);
    conf.get("subdataset", _subDataSet);
    conf.get("interpolation", "nearest", _interpolation, Image::NEAREST);
    conf.get("interpolation", "average", _interpolation, Image::AVERAGE);
    conf.get("interpolation", "bilinear", _interpolation, Image::BILINEAR);
    conf.get("interpolation", "cubic", _interpolation, Image::CUBIC);
    conf.get("interpolation", "cubicspline", _interpolation, Image::CUBICSPLINE);
    conf.get("coverage_uses_palette_index", _coverageUsesPaletteIndex);
    conf.get("single_threaded", _singleThreaded);

    setRenderType(RENDERTYPE_TERRAIN_SURFACE);
}

Config
GDALImageLayer::getConfig() const
{
    Config conf = super::getConfig();
    conf.set("url", _uri);
    conf.set("connection", _connection);
    conf.set("subdataset", _subDataSet);
    conf.set("interpolation", "nearest", _interpolation, Image::NEAREST);
    conf.set("interpolation", "average", _interpolation, Image::AVERAGE);
    conf.set("interpolation", "bilinear", _interpolation, Image::BILINEAR);
    conf.set("interpolation", "cubic", _interpolation, Image::CUBIC);
    conf.set("interpolation", "cubicspline", _interpolation, Image::CUBICSPLINE);
    conf.set("coverage_uses_palette_index", _coverageUsesPaletteIndex);
    conf.set("single_threaded", _singleThreaded);
    return conf;
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

    auto& driver = _drivers.get();

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
    if (status().failed())
        return status();

    auto& driver = _drivers.get();
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
