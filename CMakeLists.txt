cmake_minimum_required(VERSION 3.7)

project(
    rocky
    VERSION 0.0.1
    DESCRIPTION "Rocky by Pelican Mapping"
    LANGUAGES CXX C
)

# require C++17
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(vsgMacros)
vsg_setup_dir_vars()
vsg_setup_build_vars()

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# find the Vulkan SDK
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(GDAL CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyxml CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# source code
add_subdirectory(src)

vsg_add_feature_summary()
