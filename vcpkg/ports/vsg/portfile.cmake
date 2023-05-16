vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO vsg-dev/VulkanSceneGraph
    REF e775af7f08a1890ef7b5ba402474dce687790835
    SHA512 fcdabc1bca3bdf663a125d70efe5d6f1cf57304af325d4262e9343cb30246ae35ad88b4acd3fc3ae6bc9b60326c68be6fd6b138bb6e8a9e17aaf890495b4c5cc
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
