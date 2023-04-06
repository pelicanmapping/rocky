# vcpkg_from_github(
#	OUT_SOURCE_PATH SOURCE_PATH
#	REPO vsg-dev/VulkanSceneGraph
#    SHA512 9bcb146bc70e3a5ea17de81d9eb92c024bad2856a01b289cb0641af025a068bc3aa4c10aae5039f9eeb7cbea7af7844baa4777aebf4012249b8c8d1ff3fec6b3
#    REF 57c2cd59131da028eb77fc24ed6b73acd92ba503
#	HEAD_REF master
#)

find_program(GIT git)

set(GIT_URL "https://github.com/vsg-dev/VulkanSceneGraph.git")
set(GIT_REV "57c2cd59131da028eb77fc24ed6b73acd92ba503")

set(SOURCE_PATH ${CURRENT_BUILDTREES_DIR}/src/${PORT})


if(NOT EXISTS "${SOURCE_PATH}/.git")
	message(STATUS "Cloning and fetching submodules")
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
endif()



vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_install_cmake()
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsg" CONFIG_PATH "lib/cmake/vsg")
vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
