/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSElevationLayer.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

TMSElevationLayer::TMSElevationLayer() :
    super()
{
    construct(Config());
}

TMSElevationLayer::TMSElevationLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
TMSElevationLayer::construct(const Config& conf)
{
    setConfigKey("TMSElevation");

    conf.get("uri", _options.uri);
    conf.get("tms_type", _options.tmsType);
    conf.get("format", _options.format);
    conf.get("coverage", _options.coverage);
}

Config
TMSElevationLayer::getConfig() const
{
    Config conf = super::getConfig();
    conf.set("uri", _options.uri);
    conf.set("tms_type", _options.tmsType);
    conf.set("format", _options.format);
    conf.set("coverage", _options.coverage);
    return conf;
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
        uri(),
        driver_profile,
        format(),
        coverage(),
        dataExtents,
        io);

    if (status.failed())
        return status;

    if (driver_profile != profile())
    {
        setProfile(driver_profile);
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
TMSElevationLayer::createHeightfieldImplementation(
    const TileKey& key,
    const IOOptions& io) const
{
    bool invertY = (tmsType() == "google");
    const std::string suffix = "?mapbox=true"; // mapbox rgb elevation format

    // request
    auto r = _driver.read(uri(), suffix, key, invertY, io);

    if (r.status.ok())
    {
        // convert the RGB Elevation into an actual heightfield
        auto hf = Heightfield::create(r.value->width(), r.value->height());

        fvec4 pixel;
        for (unsigned y = 0; y < r.value->height(); ++y)
        {
            for (unsigned x = 0; x < r.value->width(); ++x)
            {
                r.value->read(pixel, x, y);
                float height = -10000.f + 
                    ((pixel.r * 256.0f * 256.0f + pixel.g * 256.0f + pixel.b) * 256.0f * 0.1f);
                hf->heightAt(x, y) = height;
            }
        }
        return GeoHeightfield(hf, key.extent());
    }
    else
    {
        return r.status;
    }
}
