/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Instance.h"
#include "GeoExtent.h"
#include "Profile.h"
#include "SRS.h"
#include "Threading.h"
#include "Utils.h"
#include "Version.h"
#include "json.h"
#include "weejobs.h"

ROCKY_ABOUT(rocky, ROCKY_VERSION_STRING)
ROCKY_ABOUT(weejobs, WEEJOBS_VERSION_STRING)
ROCKY_ABOUT(glm, std::to_string(GLM_VERSION_MAJOR) + "." + std::to_string(GLM_VERSION_MINOR) + "." + std::to_string(GLM_VERSION_PATCH) + "." + std::to_string(GLM_VERSION_REVISION))
ROCKY_ABOUT(nlohmann_json, std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." + std::to_string(NLOHMANN_JSON_VERSION_MINOR));

#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <cpl_conv.h>
ROCKY_ABOUT(gdal, GDAL_RELEASE_NAME)
#endif

#ifdef ROCKY_HAS_TINYXML
#include <tinyxml.h>
ROCKY_ABOUT(tinyxml, std::to_string(TIXML_MAJOR_VERSION) + "." + std::to_string(TIXML_MINOR_VERSION))
#endif

#ifdef ROCKY_HAS_GEOCODER
#include <ogr_geocoding.h>
#endif


using namespace ROCKY_NAMESPACE;

struct ROCKY_NAMESPACE::Instance::Implementation
{
    IOOptions ioOptions;
};

const Status& Instance::status() { return _global_status; }

// Static object factory map.
// Normally this would go in static.cpp, but since the registration macro (ROCKY_ADD_OBJECT_FACTORY)
// runs at static initialization time itself, we need to construct the factories map on demand.
std::unordered_map<std::string, Instance::ObjectFactory>& Instance::objectFactories()
{
    static std::unordered_map<std::string, Instance::ObjectFactory> factories;
    return factories;
}

// static object creation function:
std::shared_ptr<Object>
Instance::createObjectImpl(const std::string& name, const std::string& JSON, const IOOptions& io)
{
    auto i = objectFactories().find(util::toLower(name));
    if (i != objectFactories().end())
        return i->second(JSON, io);
    return nullptr;
}

#ifdef ROCKY_HAS_GDAL
namespace
{
    void CPL_STDCALL myCPLErrorHandler(CPLErr errClass, int errNum, const char* msg)
    {
        Log()->info("GDAL says: " + std::string(msg) + std::string(" (error ") + std::to_string(errNum) + ")");
    }
}
#endif

std::set<std::string>&
Instance::about()
{
    using About = std::set<std::string>;
    static About about;
    return about;
}

Instance::Instance()
{
    _impl = std::make_shared<Implementation>();

#ifdef ROCKY_HAS_GDAL
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
    CPLPushErrorHandler(myCPLErrorHandler);

    // Set the GDAL shared block cache size. This defaults to 5% of
    // available memory which is too high.
    GDALSetCacheMax(40 * 1024 * 1024);

#endif // ROCKY_HAS_GDAL

    // Check for some environment variables that are important to rocky apps
    //if (::getenv("PROJ_DATA") == nullptr)
    //{
    //    Log()->warn("Environment variable PROJ_DATA is not set");
    //}

    _global_status = Status_OK;
}

Instance::Instance(const Instance& rhs) :
    _impl(rhs._impl)
{
    // Tell the weejobs library how to set a thread name
    jobs::set_thread_name_function([](const char* value) {
        util::setThreadName(value);
        });
}

Instance::~Instance()
{
    jobs::shutdown();
    _global_status = Status_ServiceUnavailable;
}

IOOptions&
Instance::io()
{
    return _impl->ioOptions;
}

UID rocky::createUID()
{
    static std::atomic<unsigned> uidgen = 0;
    return uidgen++;
}

std::string rocky::json_pretty(const JSON& j)
{
    return parse_json(j).dump(4);
}
