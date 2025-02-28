include(CheckCXXSourceCompiles)

if(ROCKY_BUILD_SHARED_LIBS)
    add_definitions(-DROCKY_BUILDING_SHARED_LIBRARY)
endif()

# Create a "Version.h" header that automatically reflects the
# version variables found in the CMake configuration.
set(version_h ${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky/Version.h)

# collect all the headers in the source directory
file(GLOB HEADERS_CORE *.h tinyxml/*.h)
list(APPEND HEADERS_CORE Version.h.in)
list(APPEND HEADERS_CORE ${version_h})
file(GLOB SOURCES_CORE *.cpp tinyxml/*.cpp)

macro(remove_items from_list)
    foreach(item ${ARGN})
        list(REMOVE_ITEM ${from_list} ${CMAKE_CURRENT_SOURCE_DIR}/${item})
    endforeach()
endmacro()

file(GLOB HEADERS_CONTRIB contrib/*.h)
file(GLOB SOURCES_CONTRIB contrib/*.cpp)

# data files to install
list(APPEND DATA_FILES "${PROJECT_SOURCE_DIR}/data/fonts/times.vsgb")


# find dependecies ----

# PROJ - geospatial projections library - required
find_package(PROJ CONFIG REQUIRED)
set(ROCKY_HAS_PROJ TRUE)

# glm - vector and matrix math library - required
find_package(glm CONFIG REQUIRED)
set(ROCKY_HAS_GLM TRUE)

# spdlog - logging utility
find_package(spdlog CONFIG REQUIRED)
set(ROCKY_HAS_SPDLOG TRUE)

# json parser
if (BUILD_WITH_JSON)
    find_package(nlohmann_json CONFIG REQUIRED)
    if (nlohmann_json_FOUND)
        set(ROCKY_HAS_JSON TRUE) # Version.h.in
    endif()
endif()

# GDAL - geospatial data abstraction library
if(BUILD_WITH_GDAL)
    find_package(GDAL CONFIG REQUIRED)
    set(ROCKY_HAS_GDAL TRUE) # Version.h.in
endif()

# httplib - http/https header-only SDK (use BUILD_WITH_OPENSSL=ON for HTTPS support)
if(BUILD_WITH_HTTPLIB)
    find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
    if(CPP_HTTPLIB_INCLUDE_DIRS)
        include_directories(${CPP_HTTPLIB_INCLUDE_DIRS})
        set(ROCKY_HAS_HTTPLIB TRUE)
        # OpenSSL - for HTTPS support - optional
        # option(BUILD_WITH_OPENSSL "Use OpenSSL to support HTTPS protocol" ON)
        if (BUILD_WITH_OPENSSL)
            find_package(OpenSSL REQUIRED)
            if (OPENSSL_FOUND)
                set(ROCKY_HAS_OPENSSL TRUE)
            endif()
        endif()
    endif()
endif()

# CURL - HTTP library
if(BUILD_WITH_CURL)
    find_package(CURL CONFIG REQUIRED)
    set(ROCKY_HAS_CURL TRUE)
endif()

# sqlite3 - database - required to support some formats like mbtiles
if(BUILD_WITH_SQLITE3)
    find_package(unofficial-sqlite3 CONFIG QUIET)
    if(NOT unofficial-sqlite3_FOUND)
        # vcpkg SQLite not found; fall back to CMake find_package
        find_package(SQLite3 REQUIRED)
        set(unofficial-sqlite3_FOUND TRUE)
        add_library(unofficial::sqlite3::sqlite3 ALIAS SQLite::SQLite3)
    endif()
    if(unofficial-sqlite3_FOUND)
        set(ROCKY_HAS_SQLITE TRUE)
    endif()
endif()

# zlib - compression
if (BUILD_WITH_ZLIB)
    find_package(ZLIB REQUIRED)
    if (ZLIB_FOUND)
        set(ROCKY_HAS_ZLIB TRUE)
    endif()        
endif()

# dear imgui
if (BUILD_WITH_IMGUI)
    find_package(ImGui REQUIRED)
    if(ImGui_FOUND)
        set(IMGUI_FOUND TRUE)
        set(ROCKY_HAS_IMGUI TRUE)
    endif()            
endif()

# pthreads - for linux
if (UNIX)
    find_package(Threads REQUIRED)
endif()




# set up library dependencies
set(PUBLIC_LIBS glm::glm spdlog::spdlog)

set(PRIVATE_LIBS PROJ::proj)
    
if(GDAL_FOUND)
    if (EXISTS "${GDAL_INCLUDE_DIR}/ogr_geocoding.h")
        set(ROCKY_HAS_GEOCODER TRUE) # Version.h.in
    endif()
    list(APPEND PRIVATE_LIBS GDAL::GDAL)
endif()

if(CURL_FOUND)
    list(APPEND PRIVATE_LIBS CURL::libcurl)
endif()

if (OPENSSL_FOUND)
    list(APPEND PRIVATE_LIBS OpenSSL::SSL OpenSSL::Crypto)
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

if(unofficial-sqlite3_FOUND AND ZLIB_FOUND)
    set(ROCKY_HAS_MBTILES TRUE)
endif()

if(ImGui_FOUND)
    list(APPEND PRIVATE_LIBS imgui::imgui)
endif()

if (ROCKY_SUPPORTS_AZURE)
    set(ROCKY_HAS_AZURE TRUE)
endif()

if (ROCKY_SUPPORTS_BING)
    set(ROCKY_HAS_BING TRUE)
endif()

if (ROCKY_SUPPORTS_TMS)
    set(ROCKY_HAS_TMS TRUE)
endif()

if(UNIX)
    list(APPEND PUBLIC_LIBS Threads::Threads)
endif()


# VSG - rendering implementation
if(ROCKY_RENDERER_VSG)
    file(GLOB HEADERS_VSG vsg/*.h)
    file(GLOB SOURCES_VSG vsg/*.cpp)
    
    file(GLOB HEADERS_VSG_TERRAIN vsg/terrain/*.h)
    file(GLOB SOURCES_VSG_TERRAIN vsg/terrain/*.cpp)
    
    file(GLOB HEADERS_VSG_ECS vsg/ecs/*.h)
    file(GLOB SOURCES_VSG_ECS vsg/ecs/*.cpp)

    if(ImGui_FOUND)
        file(GLOB HEADERS_VSG_IMGUI vsg/imgui/*.h)
        file(GLOB SOURCES_VSG_IMGUI vsg/imgui/*.cpp)
    endif()
    
    file(GLOB SHADERS_VSG vsg/shaders/*)

    find_package(vsg CONFIG REQUIRED)
    find_package(EnTT CONFIG REQUIRED)
    list(APPEND PUBLIC_LIBS EnTT::EnTT vsg::vsg)
    
    set(ROCKY_HAS_VSG TRUE)
    set(ROCKY_HAS_ENTT TRUE)

    find_package(vsgXchange CONFIG) # not REQUIRED
    if (vsgXchange_FOUND)
         list(APPEND PRIVATE_LIBS vsgXchange::vsgXchange)
         set(ROCKY_HAS_VSGXCHANGE TRUE)
    endif()
    
    set(ROCKY_MAX_NUMBER_OF_VIEWS "4" CACHE STRING "Maximum number of VSG views to support")
    
    # validate ROCKY_MAX_NUMBER_OF_VIEWS
    if(NOT ROCKY_MAX_NUMBER_OF_VIEWS MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Nice try. ROCKY_MAX_NUMBER_OF_VIEWS must be an integer.")
    endif()
    if (ROCKY_MAX_NUMBER_OF_VIEWS LESS 1)
        message(FATAL_ERROR "Nice try. ROCKY_MAX_NUMBER_OF_VIEWS must be greater than zero.")
    endif()
endif()


# generates the version and support header
# all the ROCKY_HAS_* values go into here.
configure_file(Version.h.in ${version_h})


# Put it all together
set(LIBRARIES PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})

if(ROCKY_BUILD_SHARED_LIBS)
    set(BUILD_TYPE SHARED)
else()
    set(BUILD_TYPE STATIC)
endif()

add_library(rocky ${BUILD_TYPE}
    ${HEADERS_CORE} ${SOURCES_CORE}
    ${HEADERS_CONTRIB} ${SOURCES_CONTRIB}    
    ${HEADERS_VSG} ${SOURCES_VSG}
    ${HEADERS_VSG_TERRAIN} ${SOURCES_VSG_TERRAIN}
    ${HEADERS_VSG_ECS} ${SOURCES_VSG_ECS}
    ${HEADERS_VSG_IMGUI} ${SOURCES_VSG_IMGUI}
    ${SHADERS_VSG}
)

# For windows import/export
if(NOT ROCKY_BUILD_SHARED_LIBS)
    target_compile_definitions(rocky PUBLIC ROCKY_STATIC)
endif()

if(ROCKY_APPEND_SO_VERSION)
    set_target_properties(rocky PROPERTIES VERSION ${PROJECT_VERSION_ABI} SOVERSION ${PROJECT_VERSION_ABI})
endif()

# makes files appear under a "rocky" folder in the IDE.
set_target_properties(rocky PROPERTIES FOLDER "rocky")

# enables multi-processor builds in MSVC.
if(MSVC)
    target_compile_options(rocky PRIVATE "/MP")
endif()

# allows the use cmake-generated include files when building (e.g. Version.h)
include_directories(${ROCKY_BUILDTIME_INCLUDE_DIR}/rocky)

# enables folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Sources" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES_CORE} ${SOURCES_CONTRIB} ${SOURCES_VSG} ${SOURCES_VSG_TERRAIN} ${SOURCES_VSG_ECS} ${SOURCES_VSG_IMGUI})
assign_source_groups("Headers" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS_CORE} ${HEADERS_CONTRIB} ${HEADERS_VSG} ${HEADERS_VSG_TERRAIN} ${HEADERS_VSG_ECS} ${HEADERS_VSG_IMGUI})
assign_source_groups("Shaders" "${CMAKE_CURRENT_SOURCE_DIR}" ${SHADERS_VSG})

# sets up versions and position independent code that is required for unix platforms
set_property(TARGET rocky PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET rocky PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET rocky PROPERTY CXX_STANDARD 17)

target_link_libraries(rocky ${LIBRARIES})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(rocky INTERFACE ROCKY_SHARED_LIBRARY)
endif()

# install the export cmake files so other projects can import us
rocky_install_export_files(rocky)

# install the libraries, and export to a targets-file that will be
# used to make the *config.cmake file that others can use to bring
# Rocky into their projects.
install(
    TARGETS rocky
    EXPORT rockyTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

# installs the default headers.
install(FILES ${HEADERS_CORE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky)
install(FILES ${HEADERS_CONTRIB} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky/contrib)
install(FILES ${DATA_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/rocky/data)

# installs the VSG headers and shaders.
if(ROCKY_RENDERER_VSG)
    install(FILES ${HEADERS_VSG} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky/vsg)
    install(FILES ${HEADERS_VSG_TERRAIN} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky/vsg/terrain)
    install(FILES ${HEADERS_VSG_ECS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky/vsg/ecs)
    install(FILES ${HEADERS_VSG_IMGUI} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rocky/vsg/imgui)
    install(FILES ${SHADERS_VSG} ${SPIRV_SHADERS} DESTINATION ${CMAKE_INSTALL_DATADIR}/rocky/shaders)
endif()
