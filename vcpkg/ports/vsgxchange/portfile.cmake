vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
    SHA512 84047824eb39c202b24050eec13b9ccc93116f3a02f643fe32f17bf422bd7d44f96fb2cdc3b03c782a1262f342c91a7509f08bcf2683c403bfc8842be99e67b3
	REF vsgXchange-1.0.1
	HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsgxchange" CONFIG_PATH "lib/cmake/vsgXchange")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
