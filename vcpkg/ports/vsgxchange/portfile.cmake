vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/vsgXchange
    SHA512 2d88c37e962a46de09997a1b0aac9adda91eff8adeb6ae0a4d36364019fbf1c5769034bb2a9e122174e33d940ecb5d43d7cd03e020e748b4470a52a932897829
	REF 883f88744148fe2b446a4fadcaffc08b7506e879
	HEAD_REF master
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
