cmake_minimum_required(VERSION 3.10.0)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was rocky-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set(ROCKY_VERSION 0.9.6)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(ROCKY_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include")
set(ROCKY_SHARE_DIR "${PACKAGE_PREFIX_DIR}/share/rocky")
set_and_check(ROCKY_BUILD_DIR "${PACKAGE_PREFIX_DIR}")

include(CMakeFindDependencyMacro)

# Find public dependencies
find_dependency(glm CONFIG)
find_dependency(spdlog CONFIG)
find_dependency(EnTT CONFIG)
find_dependency(vsg CONFIG)

# Find private dependencies, for linking to static library
if(NOT ON)
    find_dependency(PROJ CONFIG)
    if()
        find_dependency(vsgXchange CONFIG)
    endif()
    if(TRUE)
        find_dependency(GDAL CONFIG)
    endif()
    if(TRUE)
        find_dependency(nlohmann_json CONFIG)
    endif()
endif()

if(NOT TARGET "rocky")
  include("${CMAKE_CURRENT_LIST_DIR}/rocky-targets.cmake")
endif()

set(ROCKY_FOUND TRUE)
