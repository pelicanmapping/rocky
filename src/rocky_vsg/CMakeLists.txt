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
file(GLOB HEADERS *.h)
file(GLOB HEADERS_ENGINE engine/*.h)
file(GLOB SOURCES *.cpp)
file(GLOB SOURCES_ENGINE engine/*.cpp)
file(GLOB SHADERS shaders/*)

file(GLOB SHADERS_TO_COMPILE shaders/*.vert shaders/*.frag shaders/*.glsl)

# create the target:
add_library(${TARGET} 
    ${HEADERS} ${HEADERS_ENGINE} 
    ${SOURCES} ${SOURCES_ENGINE}
    ${SHADERS})

#set(INLINE_GLSL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/terrain.sdk.glsl)

# Compile shaders into SPIRV
#foreach(file ${SHADERS_TO_COMPILE})
#    compile_shader(${file})
#endforeach()
#add_custom_target(rocky_vsg_shaders ALL DEPENDS ${SPIRV_SHADERS})

find_package(vsg CONFIG REQUIRED)

# Lib dependencies
set(LIBRARIES PUBLIC
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
rocky_assign_folders("Headers" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS} ${HEADERS_ENGINE})
rocky_assign_folders("Sources" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES} ${SOURCES_ENGINE})
rocky_assign_folders("Shaders" "${CMAKE_CURRENT_SOURCE_DIR}" ${SHADERS})

# Versioning and Language
set_property(TARGET ${TARGET} PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)

target_compile_definitions(${TARGET} PRIVATE ${EXTRA_DEFINES})

target_link_libraries(${TARGET} ${LIBRARIES})

install(
    TARGETS ${TARGET}
    EXPORT rockyTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

if (BUILD_SHARED_LIBS)
    target_compile_definitions(${TARGET} INTERFACE ROCKY_SHARED_LIBRARY)
endif()

install(
    FILES ${HEADERS}
    DESTINATION include/rocky_vsg )
    
install(
    FILES ${HEADERS_ENGINE}
    DESTINATION include/rocky_vsg/engine)

install(
    FILES ${SHADERS} ${SPIRV_SHADERS}
    DESTINATION share/shaders )
