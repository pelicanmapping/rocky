cmake_minimum_required(VERSION 3.7)


# if using vcpkg, configure overlay ports (that are not available in vcpkg prime)
if (${CMAKE_TOOLCHAIN_FILE} MATCHES ".*vcpkg.cmake.*")
    message(STATUS "Building with vcpkg toolchain.")
    set(USING_VCPKG ON)
    set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/ports)
endif()

project(
    rocky
    VERSION 0.1.0
    DESCRIPTION "Rocky by Pelican Mapping"
    HOMEPAGE_URL https://github.com/pelicanmapping/rocky
    LANGUAGES CXX C
)

# please update this with each ABI change!!
set(PROJECT_VERSION_ABI 100)

# require C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(setup_macros)
setup_dir_vars()
setup_build_vars()

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# temporary folder to hold build-time header compilation (.in files built with compile_files)
set(ROCKY_BUILDTIME_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build_include")
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR})

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# PROJ - geospatial projections library - required
find_package(PROJ CONFIG REQUIRED)

# glm - vector and matrix math library - required
find_package(glm CONFIG REQUIRED)


option(ROCKY_SUPPORTS_PROFILING "Build with performance profiling" OFF)
option(ROCKY_SUPPORTS_HTTP "Support HTTP (reuqires httplib)" ON)
option(ROCKY_SUPPORTS_HTTPS "Support HTTPS (requires openssl)" ON)
option(ROCKY_SUPPORTS_TMS "Support OSGeo TileMapService" ON)
option(ROCKY_SUPPORTS_GDAL "Support GeoTIFF and other GDAL formats (required gdal)" ON)
# option(ROCKY_SUPPORTS_WMS "Support OGC WebMapService (requires httplib)" OFF)
# option(ROCKY_SUPPORTS_WMTS "Support OGC Web Map Tile Service (requires..?)" OFF)
# option(ROCKY_SUPPORTS_MBTILES "Support MapBox MBTiles (requires sqlite3)" OFF)

set(BUILD_WITH_JSON ON)

if(ROCKY_SUPPORTS_PROFILING)
    set(BUILD_WITH_TRACY ON)
endif()

if(ROCKY_SUPPORTS_HTTP)
    set(BUILD_WITH_HTTPLIB ON)
endif()

if (ROCKY_SUPPORTS_HTTPS)
    set(BUILD_WITH_HTTPLIB ON)
    set(BUILD_WITH_OPENSSL ON)
endif()

if(ROCKY_SUPPORTS_GDAL)
    add_definitions("-DROCKY_SUPPORTS_GDAL")
    set(BUILD_WITH_GDAL ON)
endif()

if (ROCKY_SUPPORTS_MBTILES)
    add_definitions("-DROCKY_SUPPORTS_MBTILES")
    set(BUILD_WITH_SQLITE ON)
endif()

if (ROCKY_SUPPORTS_TMS)
    add_definitions("-DROCKY_SUPPORTS_TMS")
    set(BUILD_WITH_TMS ON)
    set(BUILD_WITH_TINYXML ON)
endif()

if (ROCKY_SUPPORTS_WMS)
    add_definitions("-DROCKY_SUPPORTS_WMS")
    set(BUILD_WITH_HTTPLIB ON)
    set(BUILD_WITH_WMS ON)
    set(BUILD_WITH_TINYXML ON)
endif()

if (ROCKY_SUPPORTS_WMTS)
    add_definitions("-DROCKY_SUPPORTS_WMTS")
    set(BUILD_WITH_HTTPLIB ON)
    set(BUILD_WITH_TINYXML ON)
endif()



# tinyxml - xml parser
if (BUILD_WITH_TINYXML)
    find_package(tinyxml CONFIG REQUIRED)
    if (tinyxml_FOUND)
        add_definitions("-DTINYXML_FOUND")
    endif()
endif()

# json parser
if (BUILD_WITH_JSON)
    find_package(nlohmann_json CONFIG REQUIRED)
    if (nlohmann_json_FOUND)
        add_definitions("-DNLOHMANN_JSON_FOUND")
    endif()
endif()

# httplib - http/https header-only SDK (use BUILD_WITH_OPENSSL=ON for HTTPS support)
if (BUILD_WITH_HTTPLIB)
    find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
    if(CPP_HTTPLIB_INCLUDE_DIRS)
        include_directories(${CPP_HTTPLIB_INCLUDE_DIRS})
        add_definitions("-DHTTPLIB_FOUND")
        # OpenSSL - for HTTPS support - optional
        # option(BUILD_WITH_OPENSSL "Use OpenSSL to support HTTPS protocol" ON)
        if (BUILD_WITH_OPENSSL)
            find_package(OpenSSL REQUIRED)
            if (OPENSSL_FOUND)
                add_definitions("-DOPENSSL_FOUND")
            endif()
        endif()
    endif()
endif()

# GDAL - geospatial data abstraction library
if(BUILD_WITH_GDAL)
    find_package(GDAL CONFIG REQUIRED)
    if (GDAL_FOUND)
        add_definitions("-DGDAL_FOUND")
    endif()
endif()

# sqlite3 - database - required to support some formats like mbtiles
if (BUILD_WITH_SQLITE3)
    find_package(SQLITE3 CONFIG REQUIRED)
    if (SQLITE3_FOUND)
        add_definitions("-DSQLITE3_FOUND")
    endif()
endif()

# tracy - profiling tools and headers
if (BUILD_WITH_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    if (Tracy_FOUND)
        add_definitions("-DTRACY_FOUND")
    endif()
endif()

# source code
add_subdirectory(src)


# export packaging   
include(CMakePackageConfigHelpers)

set(PACKAGE_INSTALL_DIR share/rocky)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})

configure_package_config_file(
    "cmake/rocky-config.cmake.in"
    "rocky-config.cmake"
    INSTALL_DESTINATION ${PACKAGE_INSTALL_DIR}
    PATH_VARS INCLUDE_INSTALL_DIR LIBRARY_INSTALL_DIR) 

write_basic_package_version_file(
    "rocky-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(
    EXPORT rockyTargets
    FILE rocky-targets.cmake
    NAMESPACE rocky::
    DESTINATION ${PACKAGE_INSTALL_DIR} )
    
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/rocky-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/rocky-config-version.cmake"
    DESTINATION
         ${PACKAGE_INSTALL_DIR} )
         