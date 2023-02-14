/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Instance.h"
#include "Profile.h"
#include "SRS.h"
#include "Threading.h"

#ifdef GDAL_FOUND
#include <gdal.h>
#include <cpl_conv.h>
#endif

using namespace ROCKY_NAMESPACE;

// declare static globals from Profile and SRS, so the dependency order is correct

const SRS SRS::WGS84("wgs84");
const SRS SRS::ECEF("geocentric");
const SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
const SRS SRS::PLATE_CARREE("plate-carree");
const SRS SRS::EMPTY;

const Profile Profile::GLOBAL_GEODETIC("global-geodetic");
const Profile Profile::SPHERICAL_MERCATOR("spherical-mercator");
const Profile Profile::PLATE_CARREE("plate-carree");

Status Instance::_global_status(Status::GeneralError);
const Status& Instance::status() { return _global_status; }

#ifdef GDAL_FOUND
namespace
{
    void CPL_STDCALL myCPLErrorHandler(CPLErr errClass, int errNum, const char* msg)
    {
        Log::info() << "GDAL says: " << msg << " (error " << errNum << ")" << std::endl;
    }
}
#endif

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

void
Instance::addFactory(const std::string& name, ConfigFactory f)
{
    _impl->configFactories[name] = f;
}

void
Instance::addFactory(const std::string& contentType, ContentFactory f)
{
    _impl->contentFactories[contentType] = f;
}

shared_ptr<Object>
Instance::read(const Config& conf) const
{
    auto i = _impl->configFactories.find(conf.key());
    if (i == _impl->configFactories.end())
        return nullptr;

    return i->second(conf);
}
