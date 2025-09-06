/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

// Do not edit me!
// CMake generated this file from "Version.h.in" - edit that instead.

#define ROCKY_PROJECT_NAME "rocky"
#define ROCKY_PROJECT_DESCRIPTION "Rocky by Pelican Mapping"

#define ROCKY_VERSION_MAJOR 0
#define ROCKY_VERSION_MINOR 9
#define ROCKY_VERSION_REV 6
#define ROCKY_VERSION_ABI 27
#define ROCKY_SOVERSION 27
#define ROCKY_STR_NX(s) #s
#define ROCKY_STR(s) ROCKY_STR_NX(s)
#define ROCKY_COMPUTE_VERSION(major, minor, rev) ((major) * 10000 + (minor) * 100 + (rev))
#define ROCKY_VERSION_NUMBER ROCKY_COMPUTE_VERSION(ROCKY_VERSION_MAJOR, ROCKY_VERSION_MINOR, ROCKY_VERSION_REV)
#define ROCKY_VERSION_STRING ROCKY_STR(ROCKY_VERSION_MAJOR) "." ROCKY_STR(ROCKY_VERSION_MINOR) "." ROCKY_STR(ROCKY_VERSION_REV)

#define ROCKY_VERSION_BEFORE(major, minor, rev) ROCKY_VERSION_NUMBER < ROCKY_COMPUTE_VERSION(major, minor, rev)
#define ROCKY_VERSION_AT_LEAST(major, minor, rev) ROCKY_VERSION_NUMBER >= ROCKY_COMPUTE_VERSION(major, minor, rev))

// Build constants

#define ROCKY_MAX_NUMBER_OF_VIEWS 4

#define ROCKY_USE_UTF8_FILENAMES_ON_WINDOWS

// Dependency defines

#define ROCKY_HAS_GLM
#define ROCKY_HAS_PROJ
#define ROCKY_HAS_SPDLOG
#define ROCKY_HAS_JSON
#define ROCKY_HAS_HTTPLIB
#define ROCKY_HAS_OPENSSL
/* #undef ROCKY_HAS_CURL */
#define ROCKY_HAS_GDAL
#define ROCKY_HAS_SQLITE
#define ROCKY_HAS_ZLIB
#define ROCKY_HAS_MBTILES
#define ROCKY_HAS_AZURE
#define ROCKY_HAS_BING
#define ROCKY_HAS_GEOCODER
/* #undef ROCKY_HAS_IMGUI */

#define ROCKY_HAS_VSG
/* #undef ROCKY_HAS_VSGXCHANGE */
#define ROCKY_HAS_ENTT
/* #undef ROCKY_HAS_GLSLANG */
