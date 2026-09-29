vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
include("${CMAKE_CURRENT_LIST_DIR}/slughorn-version.cmake")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO AlphaPixel/slughorn
    REF "${ROCKY_SLUGHORN_REVISION}"
    SHA512 "${ROCKY_SLUGHORN_SHA512}"
)

# The same small packaging project is used by Rocky's local-source and
# FetchContent paths. Do not run upstream's Python-oriented top-level build.
file(COPY
    "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_DIR}/slughorn-config.cmake.in"
    "${CMAKE_CURRENT_LIST_DIR}/slughorn-version.cmake"
    DESTINATION "${SOURCE_PATH}/rocky-cmake")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/rocky-cmake"
    OPTIONS "-DSLUGHORN_SOURCE_DIR=${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/slughorn)
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
