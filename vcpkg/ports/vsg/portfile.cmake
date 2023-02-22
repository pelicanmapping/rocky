vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/VulkanSceneGraph
    SHA512 e9563f0b63b0ece1ed0a2a96c01c3137345c3cdd2e0e04f465b83db13a51d2ea465e286d8f2a759005c7625a7a53e434aef49aab57eebc72fbfa0537dde79f71
    REF master
	HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
