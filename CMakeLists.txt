cmake_minimum_required(VERSION 3.7)

# if using vcpkg, configure overlay ports (that are not available in vcpkg prime)
if (${CMAKE_TOOLCHAIN_FILE} MATCHES ".*vcpkg.cmake.*")
    message(STATUS "Building with vcpkg toolchain.")
    set(USING_VCPKG ON)
    set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg-ports)
endif()

# Main PROJECT definition
project(
    rocky
    VERSION 0.9.8
    DESCRIPTION "Rocky by Pelican Mapping"
    HOMEPAGE_URL https://github.com/pelicanmapping/rocky
    LANGUAGES CXX C
)

# please update this with each ABI change!!
set(PROJECT_VERSION_ABI 37)

# require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# define that we can use when checking for conditional features in the SDK build
add_definitions(-DROCKY_BUILDING_SDK)

# option to build shared or static libs
option(ROCKY_BUILD_SHARED_LIBS "Build shared libraries" ON)
mark_as_advanced(ROCKY_BUILD_SHARED_LIBS)

# whether to append the ABI version to shared library filenames
option(ROCKY_APPEND_SO_VERSION "Whether to append the ABI version to libraries" ON)
mark_as_advanced(ROCKY_APPEND_SO_VERSION)

# folder for additional cmake includes
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# temporary folder to hold build-time header compilation (.in files built with compile_files)
set(ROCKY_BUILDTIME_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build_include")
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR})

# temporary folder to hold shared resources (like shaders) for use when running from the IDE
set(ROCKY_BUILDTIME_SHARE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build_share")

# set up standard install folder names, CMAKE_INSTALL_*
# https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html
include(GNUInstallDirs)

# load utilities
include("cmake/install-export-files.cmake")

# optional modules
option(ROCKY_RENDERER_VSG "Build the Vulkan/VSG rendering engine" ON)
option(ROCKY_SUPPORTS_HTTPLIB "Support HTTP with the lightweight cpp-httplib library" ON)
option(ROCKY_SUPPORTS_CURL "Support HTTP with the CURL library" OFF)
option(ROCKY_SUPPORTS_HTTPS "Support HTTPS (requires openssl)" ON)
option(ROCKY_SUPPORTS_GDAL "Support GeoTIFF, WMS, WMTS, and other GDAL formats (requires gdal)" ON)
option(ROCKY_SUPPORTS_MBTILES "Support MBTiles databases with extended spatial profile support (requires sqlite3, zlib)" ON)
option(ROCKY_SUPPORTS_AZURE "Support Azure Maps (subscription required)" ON)
option(ROCKY_SUPPORTS_BING "Support Bing Maps (subscription required)" OFF)
option(ROCKY_SUPPORTS_IMGUI "Support Dear ImGui and build ImGui-based demos" ON)
option(ROCKY_SUPPORTS_QT "Build Qt demos" OFF)

mark_as_advanced(ROCKY_RENDERER_VSG)
mark_as_advanced(ROCKY_SUPPORTS_HTTPLIB)
mark_as_advanced(ROCKY_SUPPORTS_CURL)

option(ROCKY_USE_UTF8_FILENAMES_ON_WINDOWS "" ON)

set(BUILD_WITH_JSON ON)

if(ROCKY_SUPPORTS_GDAL)
    set(BUILD_WITH_GDAL ON)
endif()

if(ROCKY_SUPPORTS_HTTPLIB)
    set(BUILD_WITH_HTTPLIB ON)
endif()

if(ROCKY_SUPPORTS_HTTPS)
    set(BUILD_WITH_OPENSSL ON)
endif()

if (ROCKY_SUPPORTS_MBTILES)
    set(BUILD_WITH_SQLITE3 ON)
    set(BUILD_WITH_ZLIB ON)
endif()

if(ROCKY_SUPPORTS_QT)
    set(BUILD_WITH_QT ON)
endif()

if(ROCKY_SUPPORTS_CURL)
    if(BUILD_WITH_HTTPLIB)
        message(WARNING "You selected both HTTPLIB and CURL. These are mutually exclusive, so I am disabling CURL.")
    else()
        set(BUILD_WITH_CURL ON)
    endif()
endif()

if(ROCKY_SUPPORTS_IMGUI)
    set(BUILD_WITH_IMGUI ON)
endif()

# source code
add_subdirectory(src)