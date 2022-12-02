set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(rocky)
add_subdirectory(rocky_vsg)
add_subdirectory(apps)
add_subdirectory(tests)