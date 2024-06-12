cmake_minimum_required(VERSION 3.7)

# if using vcpkg, configure overlay ports (that are not available in vcpkg prime)
if (${CMAKE_TOOLCHAIN_FILE} MATCHES ".*vcpkg.cmake.*")
    message(STATUS "Building with vcpkg toolchain.")
    set(USING_VCPKG ON)
    set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/ports)
endif()

# Main PROJECT definition
project(
    rocky
    VERSION 0.3.2
    DESCRIPTION "Rocky by Pelican Mapping"
    HOMEPAGE_URL https://github.com/pelicanmapping/rocky
    LANGUAGES CXX C
)

# please update this with each ABI change!!
set(PROJECT_VERSION_ABI 105)

# require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# define that we can use when checking for conditional features in the SDK build
add_definitions(-DROCKY_BUILDING_SDK)

# option to build shared or static libs
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# folder for additional cmake includes
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# temporary folder to hold build-time header compilation (.in files built with compile_files)
set(ROCKY_BUILDTIME_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build_include")
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR})

# set up standard install folder names
# https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html
include(GNUInstallDirs)

# load utilities
include("cmake/install-export-files.cmake")

# PROJ - geospatial projections library - required
find_package(PROJ CONFIG REQUIRED)
set(ROCKY_HAS_PROJ TRUE)

# glm - vector and matrix math library - required
find_package(glm CONFIG REQUIRED)
set(ROCKY_HAS_GLM TRUE)

# spdlog - logging utility
find_package(spdlog CONFIG REQUIRED)
set(ROCKY_HAS_SPDLOG TRUE)

# optional modules
option(ROCKY_RENDERER_VSG "Build the VSG/Vulkan Rendering" ON)

option(ROCKY_SUPPORTS_HTTP "Support HTTP (reuqires httplib)" ON)
option(ROCKY_SUPPORTS_HTTPS "Support HTTPS (requires openssl)" ON)
option(ROCKY_SUPPORTS_TMS "Support OSGeo TileMapService" ON)
option(ROCKY_SUPPORTS_GDAL "Support GeoTIFF, WMS, WMTS, and other GDAL formats (requires gdal)" ON)
option(ROCKY_SUPPORTS_MBTILES "Support MBTiles databases with extended spatial profile support (requires sqlite3, zlib)" ON)
option(ROCKY_SUPPORTS_IMGUI "Build ImGui demos" ON)
option(ROCKY_SUPPORTS_QT "Build Qt demos" OFF)

set(BUILD_WITH_JSON ON)

if(ROCKY_SUPPORTS_QT)
    set(BUILD_WITH_QT ON)
endif()

if(ROCKY_SUPPORTS_HTTP)
    set(BUILD_WITH_HTTPLIB ON)
endif()

if (ROCKY_SUPPORTS_HTTPS)
    set(BUILD_WITH_HTTPLIB ON)
    set(BUILD_WITH_OPENSSL ON)
endif()

if(ROCKY_SUPPORTS_GDAL)
    set(BUILD_WITH_GDAL ON)
    set(ROCKY_HAS_GDAL TRUE)
endif()

if (ROCKY_SUPPORTS_MBTILES)
    set(BUILD_WITH_SQLITE3 ON)
    set(BUILD_WITH_ZLIB ON)
    set(ROCKY_HAS_MBTILES ON)
endif()

if (ROCKY_SUPPORTS_TMS)
    #set(BUILD_WITH_TINYXML ON)
    set(ROCKY_HAS_TMS TRUE)
endif()

# tinyxml - xml parser
#if (BUILD_WITH_TINYXML)
#    find_package(tinyxml CONFIG REQUIRED)
#    if (tinyxml_FOUND)
#        set(ROCKY_HAS_TINYXML TRUE)
#    endif()
#endif()

# json parser
if (BUILD_WITH_JSON)
    find_package(nlohmann_json CONFIG REQUIRED)
    if (nlohmann_json_FOUND)
        set(ROCKY_HAS_JSON TRUE)
    endif()
endif()

# httplib - http/https header-only SDK (use BUILD_WITH_OPENSSL=ON for HTTPS support)
if (BUILD_WITH_HTTPLIB)
    find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
    if(CPP_HTTPLIB_INCLUDE_DIRS)
        include_directories(${CPP_HTTPLIB_INCLUDE_DIRS})
        set(ROCKY_HAS_HTTPLIB TRUE)
        # OpenSSL - for HTTPS support - optional
        # option(BUILD_WITH_OPENSSL "Use OpenSSL to support HTTPS protocol" ON)
        if (BUILD_WITH_OPENSSL)
            find_package(OpenSSL REQUIRED)
            if (OPENSSL_FOUND)
                set(ROCKY_HAS_OPENSSL TRUE)
            endif()
        endif()
    endif()
endif()

# GDAL - geospatial data abstraction library
if(BUILD_WITH_GDAL)
    find_package(GDAL CONFIG REQUIRED)
endif()

# sqlite3 - database - required to support some formats like mbtiles
if (BUILD_WITH_SQLITE3)
    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    if (unofficial-sqlite3_FOUND)
        set(ROCKY_HAS_SQLITE TRUE)
    endif()
endif()

# zlib - compression
if (BUILD_WITH_ZLIB)
    find_package(ZLIB REQUIRED)
    if (ZLIB_FOUND)
        set(ROCKY_HAS_ZLIB TRUE)
    endif()        
endif()

# pthreads - for linux
if (UNIX)
    find_package(Threads REQUIRED)
endif()

# source code
add_subdirectory(src)
