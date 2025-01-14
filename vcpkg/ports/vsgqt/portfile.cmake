#vcpkg_from_github(
#	OUT_SOURCE_PATH SOURCE_PATH
#	REPO vsg-dev/vsgQt
#    SHA512 969ae0cf67aff48d4846007f5301a0c302493162408412f88836b218bbf83c493e9eb63f8b668eda6363580650b8b7d01c9991c25f2d7109c0b43379c1e219b2
#    REF ba24c5776414129d8631d8b6fe05f8860bb8566f
#	HEAD_REF master
#)

find_program(GIT git)

set(GIT_URL "https://github.com/vsg-dev/vsgQt.git")
set(GIT_REV "b1f8a9c0be249d14b9affe8f97741cdb7eb66776") # Version 0.40

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
vcpkg_cmake_config_fixup(PACKAGE_NAME "vsgqt" CONFIG_PATH "lib/cmake/vsgqt")
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(INSTALL ${SOURCE_PATH}/LICENSE.md DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
