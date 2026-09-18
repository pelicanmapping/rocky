cmake_minimum_required(VERSION 3.21)
project(rocky_slughorn_package LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${CMAKE_CURRENT_LIST_DIR}/slughorn-version.cmake")

# Build unmodified external SDK sources, not a copy embedded in Rocky. Keeping
# this recipe here also lets non-vcpkg users build/install the package directly.
foreach(_file slughorn.cpp slughorn.hpp canvas.hpp render.hpp serial.hpp)
    if(NOT EXISTS "${SLUGHORN_SOURCE_DIR}/slughorn/${_file}")
        message(FATAL_ERROR "Invalid SLUGHORN_SOURCE_DIR: missing slughorn/${_file} in '${SLUGHORN_SOURCE_DIR}'. Point it at a complete SDK checkout, or disable ROCKY_SUPPORTS_SLUGHORN.")
    endif()
endforeach()

# A local checkout may be newer than our download pin. Advertise its actual
# header version in the installed config rather than mislabeling it as the pin.
file(STRINGS "${SLUGHORN_SOURCE_DIR}/slughorn/slughorn.hpp" _version_lines
    REGEX "SLUGHORN_VERSION_(MAJOR|MINOR|PATCH)[ \t]*=[ \t]*[0-9]+")
foreach(_part MAJOR MINOR PATCH)
    if(NOT "${_version_lines}" MATCHES "SLUGHORN_VERSION_${_part}[ \t]*=[ \t]*([0-9]+)")
        message(FATAL_ERROR "Cannot determine Slughorn ${_part} version in ${SLUGHORN_SOURCE_DIR}.")
    endif()
    set(_version_${_part} "${CMAKE_MATCH_1}")
endforeach()
set(SLUGHORN_PACKAGE_VERSION "${_version_MAJOR}.${_version_MINOR}.${_version_PATCH}")
find_package(nlohmann_json CONFIG REQUIRED)

add_library(slughorn STATIC "${SLUGHORN_SOURCE_DIR}/slughorn/slughorn.cpp")
add_library(slughorn::slughorn ALIAS slughorn)
target_compile_features(slughorn PUBLIC cxx_std_20)
target_compile_definitions(slughorn PRIVATE SLUGHORN_SERIAL_IMPLEMENTATION PUBLIC SLUGHORN_HAS_SERIAL)
target_include_directories(slughorn PUBLIC
    "$<BUILD_INTERFACE:${SLUGHORN_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(slughorn PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(slughorn PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    DEBUG_POSTFIX d
    FOLDER third_party)

install(TARGETS slughorn EXPORT slughornTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    "${SLUGHORN_SOURCE_DIR}/slughorn/slughorn.hpp"
    "${SLUGHORN_SOURCE_DIR}/slughorn/canvas.hpp"
    "${SLUGHORN_SOURCE_DIR}/slughorn/render.hpp"
    "${SLUGHORN_SOURCE_DIR}/slughorn/serial.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/slughorn)
install(FILES "${SLUGHORN_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/slughorn)

set(_config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/slughorn")
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/slughorn-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slughorn-config.cmake" INSTALL_DESTINATION "${_config_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/slughorn-config-version.cmake"
    VERSION "${SLUGHORN_PACKAGE_VERSION}" COMPATIBILITY SameMinorVersion)
install(EXPORT slughornTargets NAMESPACE slughorn:: FILE slughorn-targets.cmake DESTINATION "${_config_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slughorn-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slughorn-config-version.cmake"
    DESTINATION "${_config_dir}")
