include(CMakeFindDependencyMacro)
find_dependency(slughorn CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/slughornSmokeTargets.cmake")
