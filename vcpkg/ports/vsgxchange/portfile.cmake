# vsgxchange 1.0.5
vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
    SHA512 d01e896fd95524af62753273ffd3bb671b86ef9968d6862bc1b49d0716765e8a2ff950f01d597b215da2ea4cd931fd55b202624dc31e088dbf24f678c22446ef
	REF 113466c59bb6c849c3a4b7af19bc9028b2878bf1
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
