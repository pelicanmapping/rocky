cmake_minimum_required(VERSION 3.7)

# if using vcpkg, configure overlay ports (that are not available in vcpkg prime)
set(VCPKG_OVERLAY_PORTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/ports)

project(
    rocky
    VERSION 0.0.1
    DESCRIPTION "Rocky by Pelican Mapping"
    LANGUAGES CXX C
)

# require C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(setup_macros)
setup_dir_vars()
setup_build_vars()

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)


# PROJ - geospatial projections library - required
find_package(PROJ CONFIG REQUIRED)

# glm - vector and matrix math library - required
find_package(glm CONFIG REQUIRED)

find_package(tinyxml CONFIG REQUIRED)

find_package(nlohmann_json CONFIG REQUIRED)

option(BUILD_WITH_OPENSSL "Use OpenSSL to support HTTPS protocol" ON)
if (BUILD_WITH_OPENSSL)
    find_package(OpenSSL)
    if (OPENSSL_FOUND)
        add_definitions("-DOPENSSL_FOUND")
    endif()
endif()

# GDAL - geospatial data abstraction library - optional
option(BUILD_WITH_GDAL "Use GDAL to load GeoTIFF and other geodata formats" ON)
if(BUILD_WITH_GDAL)
    find_package(GDAL CONFIG)
    if (GDAL_FOUND)
        add_definitions("-DGDAL_FOUND")
    endif()
endif()

# httplib - http/https header-only SDK
# (link to libssl and libcrypto for HTTPS support)
# https://github.com/yhirose/cpp-httplib
find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
if(CPP_HTTPLIB_INCLUDE_DIRS)
    include_directories(${CPP_HTTPLIB_INCLUDE_DIRS})
    add_definitions("-DHTTPLIB_FOUND")
endif()

# source code
add_subdirectory(src)
