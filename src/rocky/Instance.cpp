/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Instance.h"
#include "Threading.h"
#include <gdal.h>
#include <cpl_conv.h>

#include "GDALLayers.h"

using namespace rocky;

Instance::Instance()
{
    ROCKY_INFO << "Hello, world." << std::endl;

    OGRRegisterAll();
    GDALAllRegister();

#ifdef ROCKY_USE_UTF8_FILENAME
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
#else
    // support Chinese character in the file name and attributes in ESRI's shapefile
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
#endif
    CPLSetConfigOption("SHAPE_ENCODING", "");

#if GDAL_VERSION_MAJOR>=3
    CPLSetConfigOption("OGR_CT_FORCE_TRADITIONAL_GIS_ORDER", "YES");
#endif

    // Redirect GDAL/OGR console errors to our own handler
    //CPLPushErrorHandler(myCPLErrorHandler);

    // Set the GDAL shared block cache size. This defaults to 5% of
    // available memory which is too high.
    GDALSetCacheMax(40 * 1024 * 1024);

    // register known layer types.
    addFactory("gdalimage",
        [](const Config& conf) { return GDALImageLayer::create(conf); });

    addFactory("gdalelevation",
        [](const Config& conf) { return GDALElevationLayer::create(conf); });
}

Instance::~Instance()
{
    //nop
    ROCKY_INFO << "Goodbye." << std::endl;
}

UID rocky::createUID()
{
    static std::atomic<unsigned> uidgen = 0;
    return uidgen++;
}

void
Instance::addFactory(const std::string& name, ConfigFactory f)
{
    _configFactories[name] = f;
}

void
Instance::addFactory(const std::string& contentType, ContentFactory f)
{
    _contentFactories[contentType] = f;
}

shared_ptr<Object>
Instance::read(const Config& conf) const
{
    auto i = _configFactories.find(conf.key());
    if (i == _configFactories.end())
        return nullptr;

    return i->second(conf);
}
