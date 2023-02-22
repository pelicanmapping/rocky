vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/VulkanSceneGraph
    SHA512 287a01fbf5fe3f829225d63301c9daf3c3b108184fa2e77794307305b3d284b1d206c49ca5db4ea64d17aabbc571aef70bba9fab6c10ef7fe2426e1b107b1f93
    REF 3866c2e3da80a2d4e8147a96dc4acfdab6c7c57b
	HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
