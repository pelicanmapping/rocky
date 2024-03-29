include(CheckCXXSourceCompiles)

if(ROCKY_BUILD_STATIC)
    add_definitions(-DROCKY_LIBRARY_STATIC)
else()
    add_definitions(-DROCKY_LIBRARY)
endif()

# Create a "Version.h" header that automatically reflects the
# version variables found in the CMake configuration.
set(version_h ${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky/Version.h)

# collect all the headers in the source directory
file(GLOB HEADERS_CORE *.h)
list(APPEND HEADERS_CORE ${version_h})
file(GLOB SOURCES_CORE *.cpp)

macro(remove_items from_list)
    foreach(item ${ARGN})
        list(REMOVE_ITEM ${from_list} ${CMAKE_CURRENT_SOURCE_DIR}/${item})
    endforeach()
endmacro()

if(NOT ROCKY_SUPPORTS_PROFILING)
    remove_items(HEADERS_CORE "Profiling.h")
endif()
        
if(NOT ROCKY_HAS_GDAL)
    remove_items(HEADERS_CORE "GDAL.h;GDALImageLayer.h;GDALElevationLayer.h")
    remove_items(SOURCES_CORE "GDAL.cpp;GDALImageLayer.cpp;GDALElevationLayer.cpp")
endif()

if(NOT ROCKY_HAS_TMS)
    remove_items(HEADERS_CORE "TMS.h;TMSImageLayer.h;TMSElevationLayer.h")
    remove_items(SOURCES_CORE "TMS.cpp;TMSImageLayer.cpp;TMSElevationLayer.cpp")
endif()

if(NOT ROCKY_HAS_MBTILES)
    remove_items(HEADERS_CORE "MBTiles.h;MBTilesImageLayer.h;MBTilesElevationLayer.h")
    remove_items(SOURCES_CORE "MBTiles.cpp;MBTilesImageLayer.cpp;MBTilesElevationLayer.cpp")
endif()

file(GLOB HEADERS_CONTRIB contrib/*.h)
file(GLOB SOURCES_CONTRIB contrib/*.cpp)

# set up library dependencies
set(PUBLIC_LIBS glm::glm spdlog::spdlog)

set(PRIVATE_LIBS PROJ::proj)
    
if(Tracy_FOUND)
    list(APPEND PRIVATE_LIBS Tracy::TracyClient)
endif()
if(GDAL_FOUND)
    list(APPEND PRIVATE_LIBS GDAL::GDAL)
endif()
if (OPENSSL_FOUND)
    list(APPEND PRIVATE_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()
if (tinyxml_FOUND)
    list(APPEND PRIVATE_LIBS unofficial-tinyxml::unofficial-tinyxml)
endif()
if (nlohmann_json_FOUND)
    list(APPEND PRIVATE_LIBS nlohmann_json::nlohmann_json)
endif()
if (unofficial-sqlite3_FOUND)
    list(APPEND PRIVATE_LIBS unofficial::sqlite3::sqlite3)
endif()
if (ZLIB_FOUND)
    list(APPEND PRIVATE_LIBS ZLIB::ZLIB)
endif()

if(UNIX)
    list(APPEND PUBLIC_LIBS Threads::Threads)
endif()

# VSG - rendering implementation
if(ROCKY_RENDERER_VSG)
    file(GLOB HEADERS_VSG vsg/*.h)
    file(GLOB SOURCES_VSG vsg/*.cpp)
    file(GLOB HEADERS_VSG_ENGINE vsg/engine/*.h)
    file(GLOB SOURCES_VSG_ENGINE vsg/engine/*.cpp)
    file(GLOB SHADERS_VSG vsg/shaders/*)

    find_package(vsg CONFIG REQUIRED)
    find_package(glslang CONFIG REQUIRED)
    find_package(EnTT CONFIG REQUIRED)
    find_package(vsgXchange CONFIG) # not REQUIRED
    list(APPEND PUBLIC_LIBS EnTT::EnTT vsg::vsg)
    
    set(ROCKY_HAS_VSG TRUE)
    set(ROCKY_HAS_ENTT TRUE)
    set(ROCKY_HAS_GLSLANG TRUE)
endif()

# Compile shaders into SPIRV - revisit this later.
# file(GLOB SHADERS_TO_COMPILE vsg/shaders/*.vert vsg/shaders/*.frag vsg/shaders/*.glsl)
#foreach(file ${SHADERS_TO_COMPILE})
#    compile_shader(${file})
#endforeach()
#add_custom_target(rocky_vsg_shaders ALL DEPENDS ${SPIRV_SHADERS})

# Put it all together
set(LIBRARIES PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})


add_library(rocky
    ${HEADERS_CORE} ${SOURCES_CORE}
    ${HEADERS_CONTRIB} ${SOURCES_CONTRIB}    
    ${HEADERS_VSG} ${SOURCES_VSG}
    ${HEADERS_VSG_ENGINE} ${SOURCES_VSG_ENGINE}
    ${SHADERS_VSG}
)

if (vsgXchange_FOUND)
     target_compile_definitions(rocky PRIVATE VSGXCHANGE_FOUND)
     list(APPEND LIBRARIES vsgXchange::vsgXchange)
     set(ROCKY_HAS_VSGXCHANGE TRUE)
endif()

# generate the version and support header
configure_file(Version.h.in ${version_h})


set_target_properties(rocky PROPERTIES FOLDER "rocky")

if(MSVC)
    target_compile_options(rocky PRIVATE "/MP")
endif()

# allows the use cmake-generated include files when building (e.g. Version.h)
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky)

# enable folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Sources" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES_CORE} ${SOURCES_CONTRIB} ${SOURCES_VSG} ${SOURCES_VSG_ENGINE})
assign_source_groups("Headers" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS_CORE} ${HEADERS_CONTRIB} ${HEADERS_VSG} ${HEADERS_VSG_ENGINE})
assign_source_groups("Shaders" "${CMAKE_CURRENT_SOURCE_DIR}" ${SHADERS_VSG})

# set up versions and position independent code that is required for unix platforms
set_property(TARGET rocky PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET rocky PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET rocky PROPERTY CXX_STANDARD 17)

target_link_libraries(rocky ${LIBRARIES})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(rocky INTERFACE ROCKY_SHARED_LIBRARY)
endif()

# install the export cmake files so other projects can import us
rocky_install_export_files(rocky)

# install the libraries
install(
    TARGETS rocky
    EXPORT rockyTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# install the headers files
install(FILES ${HEADERS_CORE} DESTINATION include/rocky)
install(FILES ${HEADERS_CONTRIB} DESTINATION include/rocky/contrib)

if(ROCKY_RENDERER_VSG)
    install(FILES ${HEADERS_VSG} DESTINATION include/rocky/vsg)
    install(FILES ${HEADERS_VSG_ENGINE} DESTINATION include/rocky/vsg/engine)
    install(FILES ${SHADERS_VSG} ${SPIRV_SHADERS} DESTINATION share/shaders)
endif()
