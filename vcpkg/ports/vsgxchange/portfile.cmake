vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
    SHA512 fdaf3b0919a0f5dc08b976f42caa6acb6fc61d3055b2e67260770f61c89c6984209dbc9bf35ab6fabeb0eb6b18520b6e3e1274fcd3b779ff601e1606ad52cbb3
	REF 8ed70edbc4b18f9521537f8a445006f68301ad79
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
