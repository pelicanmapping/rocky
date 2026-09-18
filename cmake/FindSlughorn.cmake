# Discovery only. ResolveSlughorn.cmake owns the explicit source override and
# automatic FetchContent fallback; find_package alone never downloads anything.
if(NOT TARGET slughorn::slughorn)
    include("${CMAKE_CURRENT_LIST_DIR}/../vcpkg-ports/slughorn/slughorn-version.cmake")
    find_package(slughorn ${ROCKY_SLUGHORN_VERSION} CONFIG QUIET)
endif()
set(_Slughorn_target_found FALSE)
if(TARGET slughorn::slughorn)
    set(_Slughorn_target_found TRUE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Slughorn
    REQUIRED_VARS _Slughorn_target_found
    VERSION_VAR slughorn_VERSION)
unset(_Slughorn_target_found)
