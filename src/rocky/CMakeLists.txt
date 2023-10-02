include(CheckCXXSourceCompiles)

if(ROCKY_BUILD_STATIC)
    add_definitions(-DROCKY_LIBRARY_STATIC)
else()
    add_definitions(-DROCKY_LIBRARY)
endif()

# Create a "Version.h" header that automatically reflects the
# version variables found in the CMake configuration.
set(version_h ${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky/Version.h)
configure_file(Version.h.in ${version_h})

# collect all the headers in the source directory
file(GLOB HEADERS *.h)
list(APPEND HEADERS ${version_h})
file(GLOB SOURCES *.cpp)

macro(remove_items from_list)
    foreach(item ${ARGN})
        list(REMOVE_ITEM ${from_list} ${CMAKE_CURRENT_SOURCE_DIR}/${item})
    endforeach()
endmacro()

if (NOT ROCKY_SUPPORTS_PROFILING)
    remove_items(HEADERS "Profiling.h")
endif()
        
if(NOT ROCKY_SUPPORTS_GDAL)
    remove_items(HEADERS "GDAL.h;GDALImageLayer.h;GDALElevationLayer.h")
    remove_items(SOURCES "GDAL.cpp;GDALImageLayer.cpp;GDALElevationLayer.cpp")
endif()

if(NOT ROCKY_SUPPORTS_TMS)
    remove_items(HEADERS "TMS.h;TMSImageLayer.h;TMSElevationLayer.h")
    remove_items(SOURCES "TMS.cpp;TMSImageLayer.cpp;TMSElevationLayer.cpp")
endif()

if(NOT ROCKY_SUPPORTS_MBTILES)
    remove_items(HEADERS "MBTiles.h;MBTilesImageLayer.h;MBTilesElevationLayer.h")
    remove_items(SOURCES "MBTiles.cpp;MBTilesImageLayer.cpp;MBTilesElevationLayer.cpp")
endif()

file(GLOB HEADERS_CONTRIB contrib/*.h)
file(GLOB SOURCES_CONTRIB contrib/*.cpp)

# set up library dependencies
set(PUBLIC_LIBS glm::glm  spdlog::spdlog)

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

set(LIBRARIES PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})


add_library(rocky
    ${HEADERS} ${SOURCES}
    ${HEADERS_CONTRIB} ${SOURCES_CONTRIB})

set_target_properties(rocky PROPERTIES FOLDER "rocky")

if(MSVC)
    target_compile_options(rocky PRIVATE "/MP")
endif()

# allows the use cmake-generated include files when building (e.g. Version.h)
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky)

# enable folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES} ${SOURCES_CONTRIB})
assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS} ${HEADERS_CONTRIB})

# set up versions and position independent code that is required for unix platforms
set_property(TARGET rocky PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET rocky PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET rocky PROPERTY CXX_STANDARD 17)

target_link_libraries(rocky ${LIBRARIES})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(rocky INTERFACE ROCKY_SHARED_LIBRARY)
endif()

# install the libraries
install(
    TARGETS rocky
    EXPORT rockyTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# install the HEADER files
install(FILES ${HEADERS} DESTINATION include/rocky)
install(FILES ${HEADERS_CONTRIB} DESTINATION include/rocky/contrib)

