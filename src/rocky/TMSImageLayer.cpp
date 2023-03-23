/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TMSImageLayer.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::TMS;

#undef LC
#define LC "[TMS] "

TMSImageLayer::TMSImageLayer() :
    super()
{
    construct(Config());
}

TMSImageLayer::TMSImageLayer(const Config& conf) :
    super(conf)
{
    construct(conf);
}

void
TMSImageLayer::construct(const Config& conf)
{
    setConfigKey("TMSImage");

    conf.get("uri", _options.uri);
    conf.get("tms_type", _options.tmsType);
    conf.get("format", _options.format);
}

Config
TMSImageLayer::getConfig() const
{
    Config conf = super::getConfig();
    conf.set("uri", _options.uri);
    conf.set("tms_type", _options.tmsType);
    conf.set("format", _options.format);
    return conf;
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
TMSImageLayer::closeImplementation()
{
    _driver.close();
    super::closeImplementation();
}

Result<GeoImage>
TMSImageLayer::createImageImplementation(
    const TileKey& key,
    const IOOptions& io) const
{
    bool invertY = (tmsType() == "google");

    auto r = _driver.read(uri(), {}, key, invertY, io);

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

#if 0
//........................................................................

Config
TMSElevationLayer::Options::getConfig() const
{
    Config conf = ElevationLayer::Options::getConfig();
    writeTo(conf);
    return conf;
}

void
TMSElevationLayer::Options::fromConfig(const Config& conf)
{
    readFrom(conf);
}


//........................................................................

REGISTER_OSGEARTH_LAYER(tmselevation, TMSElevationLayer);

OE_LAYER_PROPERTY_IMPL(TMSElevationLayer, URI, URL, url);
OE_LAYER_PROPERTY_IMPL(TMSElevationLayer, std::string, TMSType, tmsType);
OE_LAYER_PROPERTY_IMPL(TMSElevationLayer, std::string, Format, format);

void
TMSElevationLayer::init()
{
    ElevationLayer::init();
}

Status
TMSElevationLayer::openImplementation()
{
    Status parent = ElevationLayer::openImplementation();
    if (parent.isError())
        return parent;

    // Create an image layer under the hood. TMS fetch is the same for image and
    // elevation; we just convert the resulting image to a heightfield
    _imageLayer = new TMSImageLayer(options());

    // Initialize and open the image layer
    _imageLayer->setReadOptions(getReadOptions());
    Status status;
    if (_writingRequested)
    {
        status = _imageLayer->openForWriting();
    }
    else
    {
        status = _imageLayer->open();
    }

    if (status.isError())
        return status;

    setProfile(_imageLayer->getProfile());
    DataExtentList dataExtents;
    _imageLayer->getDataExtents(dataExtents);
    setDataExtents(dataExtents);

    return Status::NoError;
}

Status
TMSElevationLayer::closeImplementation()
{
    if (_imageLayer.valid())
    {
        _imageLayer->close();
        _imageLayer = NULL;
    }
    return ElevationLayer::closeImplementation();
}

GeoHeightField
TMSElevationLayer::createHeightFieldImplementation(const TileKey& key, ProgressCallback* progress) const
{

    if (_imageLayer.valid() == false ||
        !_imageLayer->isOpen())
    {
        return GeoHeightField::INVALID;
    }

    // Make an image, then convert it to a heightfield
    GeoImage image = _imageLayer->createImageImplementation(key, progress);
    if (image.valid())
    {
        if (image.getImage()->s() > 1 && image.getImage()->t() > 1)
        {
            ImageToHeightFieldConverter conv;
            osg::HeightField* hf = conv.convert(image.getImage());
            return GeoHeightField(hf, key.getExtent());
        }
        else
        {
            return GeoHeightField::INVALID;
        }
    }
    else
    {
        return GeoHeightField(image.getStatus());
    }
}

Status
TMSElevationLayer::writeHeightFieldImplementation(
    const TileKey& key,
    const osg::HeightField* hf,
    ProgressCallback* progress) const
{
    if (_imageLayer.valid() == false ||
        !_imageLayer->isOpen())
    {
        return getStatus();
    }

    ImageToHeightFieldConverter conv;
    osg::ref_ptr<osg::Image> image = conv.convert(hf);
    return _imageLayer->writeImageImplementation(key, image.get(), progress);
}
#endif