vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/VulkanSceneGraph
    SHA512 e4894cbc7e0b9406522926866c51c879331a9fb36853b8165e989578aca07e2ff2121588ee6863120871a32c9abc7b815c2d8c8cf4b55619683cb403579ff222
    REF aa28d625229c7a673fd68a47eaad18e67e4ef2ed
	HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
