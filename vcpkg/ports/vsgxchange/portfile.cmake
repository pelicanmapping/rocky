# vsgxchange 1.0.5
vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
	REF 3bbba124702168b0676c25c8a2441b41c6e7b591
    SHA512 6eb5bb1a5ddecb49e24212e027c4c5b839004a5040c1fd2cdc8de8a51565174a15b2d3c37f38dbd0d5feb94c1f32ad9c31f1000e0d7d313006bbf689e39fce8b
	HEAD_REF master
	PATCHES vsgXchange.patch require-features.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        assimp   VSGXCHANGE_WITH_ASSIMP
        curl     VSGXCHANGE_WITH_CURL
        freetype VSGXCHANGE_WITH_FREETYPE
        openexr  VSGXCHANGE_WITH_OPENEXR
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsgxchange" CONFIG_PATH "lib/cmake/vsgXchange")
vcpkg_copy_pdbs()

vcpkg_copy_tools(TOOL_NAMES vsgconv AUTO_CLEAN)
file(REMOVE "${CURRENT_PACKAGES_DIR}/debug/bin/vsgconvd${VCPKG_TARGET_EXECUTABLE_SUFFIX}")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
