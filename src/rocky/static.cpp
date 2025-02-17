/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */

// ALL statics are instantiated in this file to control creation and dependency order.

#include "Log.h"
#include "SRS.h"
#include "Profile.h"
#include "Context.h"
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

const SRS SRS::WGS84("wgs84");
const SRS SRS::ECEF("geocentric");
const SRS SRS::SPHERICAL_MERCATOR("spherical-mercator");
const SRS SRS::PLATE_CARREE("plate-carree");
const SRS SRS::MOON("moon");
const SRS SRS::EMPTY;
std::function<void(int level, const char* msg)> SRS::projMessageCallback = nullptr;

// job system definition
WEEJOBS_INSTANCE;
