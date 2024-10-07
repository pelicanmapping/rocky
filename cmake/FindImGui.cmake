#.rst:
# Find ImGui
# -------------
#
# Finds the ImGui library. This module defines:
#
#  ImGui_FOUND                - True if ImGui is found
#  ImGui::ImGui               - ImGui interface target
#  ImGui::Sources             - ImGui source target for core functionality
#  ImGui::SourcesMiscCpp      - ImGui source target for misc/cpp
#
# Additionally these variables are defined for internal usage:
#
#  ImGui_INCLUDE_DIR          - Include dir
#
# The find module first tries to find ``imgui`` via a CMake config file (which
# is distributed this way via Vcpkg, for example). If that's found, the
# ``ImGui::ImGui`` target is an alias to it and the ``ImGui::Sources`` target
# is empty except for having ``ImGui::ImGui`` as a dependency.
#
# If ``imgui`` is not found, as a fallback it tries to find the C++ sources.
# You can supply their location via an ``IMGUI_DIR`` variable. Once found, the
# ``ImGui::ImGui`` target contains just the header file, while
# ``ImGui::Sources`` contains the source files in ``INTERFACE_SOURCES``.
#
# The ``ImGui::SourcesMiscCpp`` component, if requested, is always searched for
# in the form of C++ sources. Vcpkg doesn't distribute these.
#
# The desired usage that covers both cases is to link ``ImGui::Sources``
# ``PRIVATE``\ ly to a *single* target, which will then contain either the
# sources or be linked to the imgui library from Vcpkg; and linking
# ``ImGui::ImGui`` to this target ``PUBLIC``\ ly.
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 Jonathan Hale <squareys@googlemail.com>
#   Copyright © 2020 Tracy Ma <1991md@gmail.com>
#   Copyright © 2024 Pablo Escobar <mail@rvrs.in>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

macro(_imgui_setup_source_file source_var)
    # Hide warnings from imgui source files
    # GCC- and Clang-specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang"
        AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC") OR CORRADE_TARGET_EMSCRIPTEN)
        set_property(SOURCE ${${source_var}} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -Wno-old-style-cast")
    endif()

    # GCC-specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_property(SOURCE ${${source_var}} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -Wno-double-promotion -Wno-zero-as-null-pointer-constant")
    endif()

    mark_as_advanced(${source_var})
endmacro()

# Vcpkg distributes imgui as a library with a config file, so try that first --
# but only if IMGUI_DIR wasn't explicitly passed, in which case we'll look
# there instead
if(NOT IMGUI_DIR AND NOT TARGET imgui::imgui)
    find_package(imgui CONFIG QUIET)
endif()

# If we found imgui in VCPKG:
if(NOT IMGUI_DIR AND TARGET imgui::imgui)
    if(NOT TARGET ImGui::ImGui)
        add_library(ImGui::ImGui INTERFACE IMPORTED)
        set_property(TARGET ImGui::ImGui APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES imgui::imgui ${_IMGUI_EXTRA_LIBRARIES})

        # Retrieve include directory for FindPackageHandleStandardArgs later
        get_target_property(ImGui_INCLUDE_DIR imgui::imgui
            INTERFACE_INCLUDE_DIRECTORIES)

        add_library(ImGui::Sources INTERFACE IMPORTED)
        set_property(TARGET ImGui::Sources APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ImGui::ImGui)

        # Vcpkg also always builds imgui as static, to "fix" users that don't
        # properly define IMGUI_API on Windows to dllimport the symbols. Magnum
        # implicitly assumes that ImGui is dynamic if ImGuiIntegration is
        # dynamic, so here we need to define IMGUI_API to empty if it's static
        # to not have the library try to dllimport the symbols.
        get_target_property(ImGui_LIBRARY_TYPE imgui::imgui TYPE)
        if(ImGui_LIBRARY_TYPE STREQUAL STATIC_LIBRARY)
            set_property(TARGET ImGui::ImGui APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "IMGUI_API=")
        elseif(ImGui_LIBRARY_TYPE STREQUAL SHARED_LIBRARY)
            # Not doing anything if imgui is dynamic, as that's handled by the
            # implicit behavior (i.e., with vcpkg's default shared build both
            # ImGui and ImGuiIntegration integration would be dynamic).
            # Moreover, vcpkg seems to hardcode imgui to be static always, so
            # this should be never taken. Unless someone else creates its own
            # ImGui CMake config that excercises this other scenario, which is
            # (I think) rather unlikely.
        endif()
    endif()

# No vcpkg; so find the source files and compile them as part of the library they
# get linked to
else()
    message(STATUS "Trying to build ImGui from source")
    # Disable the find root path here, it overrides the
    # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
    # toolchains.
    find_path(ImGui_INCLUDE_DIR NAMES imgui.h
        HINTS ${IMGUI_DIR}
        NO_CMAKE_FIND_ROOT_PATH)
    mark_as_advanced(ImGui_INCLUDE_DIR)

    if(ImGui_INCLUDE_DIR)
        list(APPEND ImGui_INCLUDE_DIR "${ImGui_INCLUDE_DIR}/backends")
    endif()

    if(NOT TARGET ImGui::ImGui)
        add_library(ImGui::ImGui INTERFACE IMPORTED)
        set_property(TARGET ImGui::ImGui APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${ImGui_INCLUDE_DIR})
    endif()
    
    if(NOT TARGET ImGui::Sources)
        set(ImGui_Sources_FOUND TRUE)
        set(ImGui_SOURCES )

        foreach(_file imgui imgui_widgets imgui_draw imgui_demo imgui_tables backends/imgui_impl_vulkan)
            # Disable the find root path here, it overrides the
            # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
            # toolchains.
            find_file(ImGui_${_file}_SOURCE NAMES ${_file}.cpp
                HINTS ${IMGUI_DIR} NO_CMAKE_FIND_ROOT_PATH)

            if(NOT ImGui_${_file}_SOURCE)
                set(ImGui_Sources_FOUND FALSE)
                break()
            endif()

            list(APPEND ImGui_SOURCES ${ImGui_${_file}_SOURCE})
            _imgui_setup_source_file(ImGui_${_file}_SOURCE)
        endforeach()

        add_library(ImGui::Sources INTERFACE IMPORTED)
        
        set_property(TARGET ImGui::Sources APPEND PROPERTY
            INTERFACE_SOURCES "${ImGui_SOURCES}")
        set_property(TARGET ImGui::Sources APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ImGui::ImGui)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ImGui
    REQUIRED_VARS ImGui_INCLUDE_DIR HANDLE_COMPONENTS)
