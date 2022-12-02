include(CheckCXXSourceCompiles)

set(TARGET rocky_vsg)

include(rocky_vsg_macros.cmake)

if(ROCKY_BUILD_STATIC)
    add_definitions(-DROCKY_VSG_LIBRARY_STATIC)
else()
    add_definitions(-DROCKY_VSG_LIBRARY)
endif()

#message(STATUS "Current build folder is ${CMAKE_CURRENT_SOURCE_DIR}")

# collect all the headers in the source directory
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*)

#set(INLINE_GLSL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.sdk.glsl)

# Shaders
make_shader(
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.vert)
    
make_shader(
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.frag)
    
message(STATUS "Shaders = ${SPIRV_SHADERS}")

# Lib dependencies
set(LIBRARIES PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
    unofficial-tinyxml::unofficial-tinyxml
    vsg::vsg
    rocky
)

add_library(${TARGET} ${HEADERS} ${SOURCES} ${SHADERS})

add_custom_target(rocky_vsg_shaders ALL DEPENDS ${SPIRV_SHADERS})

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
#target_include_directories(rocky PUBLIC $<BUILD_INTERFACE:${ROCKY_SOURCE_DIR} $<BUILD_INTERFACE:${ROCKY_BINARY_DIR}/include>)

target_link_libraries(${TARGET} ${LIBRARIES})

install(TARGETS ${TARGET} ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET} INTERFACE ROCKY_SHARED_LIBRARY)
endif()

install(FILES ${HEADERS} DESTINATION include)
install(FILES ${SPIRV_SHADERS} DESTINATION share)
