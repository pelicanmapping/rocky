/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

// ALL statics are instantiated in this file to control creation and dependency order.

#include "Log.h"
#include "SRS.h"
#include "Profile.h"
#include "Instance.h"
#include "IOTypes.h"
#include "Threading.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

namespace
{
    // color codes
    const std::string COLOR_RED = "\033[31m";
    const std::string COLOR_YELLOW = "\033[33m";
    const std::string COLOR_DEFAULT = "\033[39m";
}

LogLevel Log::level = LogLevel::WARN;
LogFunction Log::g_userFunction = nullptr;
bool Log::g_logUsePrefix = true;
Log::LogStream Log::g_infoStream(LogLevel::INFO, "[rk-info] ", std::cout, COLOR_DEFAULT);
Log::LogStream Log::g_warnStream(LogLevel::WARN, "[rk-WARN] ", std::cout, COLOR_YELLOW);

const SRS SRS::WGS84("wgs84");
const SRS SRS::ECEF("geocentric");
const SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
const SRS SRS::PLATE_CARREE("plate-carree");
const SRS SRS::EMPTY;

const Profile Profile::GLOBAL_GEODETIC("global-geodetic");
const Profile Profile::SPHERICAL_MERCATOR("spherical-mercator");
const Profile Profile::PLATE_CARREE("plate-carree");

CachePolicy CachePolicy::DEFAULT;
CachePolicy CachePolicy::NO_CACHE(CachePolicy::Usage::NO_CACHE);
CachePolicy CachePolicy::CACHE_ONLY(CachePolicy::Usage::CACHE_ONLY);

Status Instance::_global_status(Status::GeneralError);

// job_scheduler statics:
std::mutex job_scheduler::_schedulers_mutex;
std::unordered_map<std::string, std::shared_ptr<job_scheduler>> job_scheduler::_schedulers;
std::unordered_map<std::string, unsigned> job_scheduler::_schedulersizes;
job_metrics job_metrics::_singleton;