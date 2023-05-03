vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO vsg-dev/VulkanSceneGraph
	SHA512 0404840599f1c023b9f5f15e067df5f59c63c87857980cbe6609c1f30c58786a1a4534bb6dbb16558f79f6be115ae3856c03411e6721e4be076cda6ff06f268c
	REF 4d59471d14e34d172d024aeb456568b1ae13f50f
	HEAD_REF master
)

# find_program(GIT git)

# set(GIT_URL "https://github.com/vsg-dev/VulkanSceneGraph.git")
# set(GIT_REV "95ae7957264de7b9016021c661865e157685fd35") # 1.0.5

# set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/src/${PORT})


# message(STATUS "Cloning and fetching submodules")

# file(REMOVE_RECURSE ${SOURCE_PATH})

# vcpkg_execute_required_process(
  # COMMAND ${GIT} clone --recurse-submodules ${GIT_URL} "${SOURCE_PATH}"
  # WORKING_DIRECTORY ${CURRENT_BUILDTREES_DIR}
  # LOGNAME clone
# )

# message(STATUS "Checkout revision ${GIT_REV}")
# vcpkg_execute_required_process(
  # COMMAND ${GIT} checkout ${GIT_REV}
  # WORKING_DIRECTORY ${SOURCE_PATH}
  # LOGNAME checkout
# )


#disabled paralell config because vsg uses git clone for glslang and can't do that in parallel
vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    DISABLE_PARALLEL_CONFIGURE	
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
