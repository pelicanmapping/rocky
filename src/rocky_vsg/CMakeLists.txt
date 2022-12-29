#include(CheckCXXSourceCompiles)

set(TARGET rocky_vsg)

include(rocky_vsg_macros.cmake)

if(ROCKY_BUILD_STATIC)
    add_definitions(-DROCKY_VSG_LIBRARY_STATIC)
else()
    add_definitions(-DROCKY_VSG_LIBRARY)
endif()

# find the Vulkan SDK
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

#message(STATUS "Current build folder is ${CMAKE_CURRENT_SOURCE_DIR}")

# collect all the headers in the source directory
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*)
file(GLOB SHADERS_TO_COMPILE
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)

# create the target:
add_library(${TARGET} ${HEADERS} ${SOURCES} ${SHADERS})

#set(INLINE_GLSL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.sdk.glsl)

# Compile shaders into SPIRV
foreach(file ${SHADERS_TO_COMPILE})
    compile_shader(${file})
endforeach()
    
message(STATUS "Shaders = ${SPIRV_SHADERS}")

add_custom_target(rocky_vsg_shaders ALL DEPENDS ${SPIRV_SHADERS})

find_package(vsg CONFIG REQUIRED)

# Lib dependencies
set(LIBRARIES PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
    unofficial-tinyxml::unofficial-tinyxml
    vsg::vsg
    rocky
)

# link in vsgXchange if we have it
find_package(vsgXchange CONFIG)
if (vsgXchange_FOUND)
     target_compile_definitions(${TARGET} PRIVATE VSGXCHANGE_FOUND)
     #target_link_libraries(rviewer )
     list(APPEND LIBRARIES vsgXchange::vsgXchange)
endif()

# IDE settings
set_target_properties(${TARGET} PROPERTIES FOLDER "rocky")
rocky_assign_folders("Headers" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})
rocky_assign_folders("Sources" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
rocky_assign_folders("Shaders" "${CMAKE_CURRENT_SOURCE_DIR}" ${SHADERS})

# Versioning and Language
set_property(TARGET ${TARGET} PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)

target_compile_definitions(${TARGET} PRIVATE ${EXTRA_DEFINES})

target_link_libraries(${TARGET} ${LIBRARIES})

install(TARGETS ${TARGET} ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET} INTERFACE ROCKY_SHARED_LIBRARY)
endif()

install(FILES ${HEADERS} DESTINATION include)
install(FILES ${SHADERS} ${SPIRV_SHADERS} DESTINATION share)
