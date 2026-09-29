include_guard(GLOBAL)

# Resolve one SDK target without exposing SDK types or compile requirements in
# Rocky's public interface. This function's scope keeps fetched source paths
# separate from the user-facing SLUGHORN_SOURCE_DIR cache override.
function(rocky_resolve_slughorn)
    set(_recipe "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vcpkg-ports/slughorn")
    include("${_recipe}/slughorn-version.cmake")

    # Honor the standard FetchContent source override as well as Rocky's shorter
    # spelling. Explicit paths are authoritative: never replace a broken path
    # with an installed package or a downloaded copy.
    if(NOT SLUGHORN_SOURCE_DIR AND FETCHCONTENT_SOURCE_DIR_SLUGHORN)
        set(SLUGHORN_SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_SLUGHORN}")
    endif()
    if(SLUGHORN_SOURCE_DIR)
        get_filename_component(SLUGHORN_SOURCE_DIR "${SLUGHORN_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        if(TARGET slughorn::slughorn)
            message(FATAL_ERROR "SLUGHORN_SOURCE_DIR cannot override an already-defined slughorn::slughorn target.")
        endif()
        message(STATUS "Slughorn: using source checkout ${SLUGHORN_SOURCE_DIR}")
    else()
        find_package(Slughorn QUIET MODULE)
        if(Slughorn_FOUND)
            message(STATUS "Slughorn: using installed/provided slughorn::slughorn")
            return()
        endif()

        message(STATUS "Slughorn: no installed package found; fetching ${ROCKY_SLUGHORN_REVISION}. If unavailable, provide SLUGHORN_SOURCE_DIR or configure with -DROCKY_SUPPORTS_SLUGHORN=OFF.")
        include(FetchContent)
        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()
        FetchContent_Declare(slughorn
            URL "https://github.com/AlphaPixel/slughorn/archive/${ROCKY_SLUGHORN_REVISION}.tar.gz"
            URL_HASH "SHA512=${ROCKY_SLUGHORN_SHA512}"
            # Populate only; our core-only recipe replaces upstream's build.
            SOURCE_SUBDIR rocky-core-only-unused)
        FetchContent_MakeAvailable(slughorn)
        set(SLUGHORN_SOURCE_DIR "${slughorn_SOURCE_DIR}")
    endif()

    add_subdirectory("${_recipe}" "${CMAKE_BINARY_DIR}/_deps/slughorn-package-build")
endfunction()
