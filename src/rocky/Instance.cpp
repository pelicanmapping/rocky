/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Instance.h"
#include "Profile.h"
#include "SRS.h"
#include "Threading.h"
#include "Utils.h"
#include "Version.h"
#include "json.h"

ROCKY_ABOUT(Rocky, ROCKY_VERSION_STRING)
ROCKY_ABOUT(glm, std::to_string(GLM_VERSION_MAJOR) + "." + std::to_string(GLM_VERSION_MINOR) + "." + std::to_string(GLM_VERSION_PATCH) + "." + std::to_string(GLM_VERSION_REVISION))
ROCKY_ABOUT(nlohmann_json, std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." + std::to_string(NLOHMANN_JSON_VERSION_MINOR));

#ifdef GDAL_FOUND
#include <gdal.h>
#include <cpl_conv.h>
ROCKY_ABOUT(GDAL, GDAL_RELEASE_NAME)
#endif

#ifdef TINYXML_FOUND
#include <tinyxml.h>
ROCKY_ABOUT(tinyxml, std::to_string(TIXML_MAJOR_VERSION) + "." + std::to_string(TIXML_MINOR_VERSION))
#endif


using namespace ROCKY_NAMESPACE;

const Status& Instance::status() { return _global_status; }

// static object factory map:
std::unordered_map<std::string, Instance::ObjectFactory> Instance::objectFactories;

// static object creation function:
shared_ptr<Object>
Instance::createObjectImpl(const std::string& name, const JSON& conf)
{
    auto i = objectFactories.find(util::toLower(name));
    if (i != objectFactories.end())
        return i->second(conf);
    return nullptr;
}

#ifdef GDAL_FOUND
namespace
{
    void CPL_STDCALL myCPLErrorHandler(CPLErr errClass, int errNum, const char* msg)
    {
        Log::info() << "GDAL says: " << msg << " (error " << errNum << ")" << std::endl;
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

#ifdef GDAL_FOUND
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

#endif // GDAL_FOUND

    // Check for some environment variables that are important to rocky apps
    if (::getenv("PROJ_DATA") == nullptr)
    {
        Log::warn() << "Environment variable PROJ_DATA is not set" << std::endl;
    }

    _global_status = StatusOK;
}

Instance::Instance(const Instance& rhs) :
    _impl(rhs._impl)
{
    //nop
}

Instance::~Instance()
{
    util::job_scheduler::shutdownAll();
    _global_status = Status(Status::GeneralError);
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
