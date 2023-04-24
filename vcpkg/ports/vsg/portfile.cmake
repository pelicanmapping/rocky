# vcpkg_from_github(
#	OUT_SOURCE_PATH SOURCE_PATH
#	REPO vsg-dev/VulkanSceneGraph
#    SHA512 0
#    REF 95ae7957264de7b9016021c661865e157685fd35
#	HEAD_REF master
#)

find_program(GIT git)

set(GIT_URL "https://github.com/vsg-dev/VulkanSceneGraph.git")
set(GIT_REV "95ae7957264de7b9016021c661865e157685fd35") # 1.0.5

set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/src/${PORT})


message(STATUS "Cloning and fetching submodules")

file(REMOVE_RECURSE ${SOURCE_PATH})

vcpkg_execute_required_process(
  COMMAND ${GIT} clone --recurse-submodules ${GIT_URL} "${SOURCE_PATH}"
  WORKING_DIRECTORY ${CURRENT_BUILDTREES_DIR}
  LOGNAME clone
)

message(STATUS "Checkout revision ${GIT_REV}")
vcpkg_execute_required_process(
  COMMAND ${GIT} checkout ${GIT_REV}
  WORKING_DIRECTORY ${SOURCE_PATH}
  LOGNAME checkout
)



vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
