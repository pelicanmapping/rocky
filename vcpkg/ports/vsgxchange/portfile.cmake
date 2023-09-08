# vsgxchange 1.0.5
vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
    SHA512 d01e896fd95524af62753273ffd3bb671b86ef9968d6862bc1b49d0716765e8a2ff950f01d597b215da2ea4cd931fd55b202624dc31e088dbf24f678c22446ef
	REF 113466c59bb6c849c3a4b7af19bc9028b2878bf1
	HEAD_REF master
	PATCHES vsgXchange.patch
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsgxchange" CONFIG_PATH "lib/cmake/vsgXchange")
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
