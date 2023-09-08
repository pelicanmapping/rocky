# vsg 1.0.9
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vsg-dev/VulkanSceneGraph
    REF 1ff7aa5de4c23aeb2201e20a89eaaa13bba77b4b
    SHA512 23e99b9c62a987bfc014cbdbd3237cfebc94f5c045ec6c65295172aebbd97ee7df478395ab89f6d3fcbac04bfac97af3a2f1c8b1463bded3442e2bcc4fda53a1
    HEAD_REF master
	PATCHES devendor-glslang.patch
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
