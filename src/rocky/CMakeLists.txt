include(CheckCXXSourceCompiles)

if(ROCKY_BUILD_STATIC)
    add_definitions(-DROCKY_LIBRARY_STATIC)
else()
    add_definitions(-DROCKY_LIBRARY)
endif()

message(STATUS "Current build folder is ${CMAKE_CURRENT_SOURCE_DIR}")

# collect all the headers in the source directory
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# set up library dependencies
set(PRIVATE_LIBS
    PROJ::proj
    nlohmann_json::nlohmann_json
    unofficial-tinyxml::unofficial-tinyxml)
    
set(PUBLIC_LIBS
    glm::glm)
    
if(GDAL_FOUND)
    list(APPEND PRIVATE_LIBS GDAL::GDAL)
endif()

if (OPENSSL_FOUND)
    list(APPEND PRIVATE_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

set(LIBRARIES PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})

add_library(rocky ${HEADERS} ${SOURCES})

set_target_properties(rocky PROPERTIES FOLDER "rocky")

# STB header-only
# find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
# target_include_directories(rocky PRIVATE ${STB_INCLUDE_DIRS})

if(MSVC)
    # ensure the libraries are all built in the lib directory
    macro(SET_OUTPUT_DIR_PROPERTY TARGET_TARGETNAME RELATIVE_OUTDIR)
        # Global properties (All generators but VS & Xcode)
        set_target_properties(${TARGET_TARGETNAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")
        set_target_properties(${TARGET_TARGETNAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")
        set_target_properties(${TARGET_TARGETNAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")

        # Per-configuration property (VS, Xcode)
        foreach(CONF ${CMAKE_CONFIGURATION_TYPES})        # For each configuration (Debug, Release, MinSizeRel... and/or anything the user chooses)
            string(TOUPPER "${CONF}" CONF)                # Go uppercase (DEBUG, RELEASE...)
            # We use "FILE(TO_CMAKE_PATH", to create nice looking paths
            set_target_properties(${TARGET_TARGETNAME} PROPERTIES "ARCHIVE_OUTPUT_DIRECTORY_${CONF}" "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")
            set_target_properties(${TARGET_TARGETNAME} PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${CONF}" "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")
            set_target_properties(${TARGET_TARGETNAME} PROPERTIES "LIBRARY_OUTPUT_DIRECTORY_${CONF}" "${OUTPUT_LIBDIR}/${RELATIVE_OUTDIR}")
        endforeach()
    endmacro()

    SET_OUTPUT_DIR_PROPERTY(rocky "")

    option(ENABLE_MP_FLAG "Adds the multi-processor flag in MSVC" ON)
    if(ENABLE_MP_FLAG)
        target_compile_options(rocky PRIVATE "/MP")
    endif()
    
endif()

# enable folders for MSVC
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# group source files and headers
assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

# set up versions and position independent code that is required for unix platforms
#set_property(TARGET rocky PROPERTY VERSION ${VSG_VERSION_MAJOR}.${VSG_VERSION_MINOR}.${VSG_VERSION_PATCH})
set_property(TARGET rocky PROPERTY SOVERSION ${ROCKY_SOVERSION})
set_property(TARGET rocky PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET rocky PROPERTY CXX_STANDARD 17)

# target_compile_definitions(rocky PRIVATE ${EXTRA_DEFINES})

target_link_libraries(rocky ${LIBRARIES})

install(TARGETS rocky ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(rocky INTERFACE ROCKY_SHARED_LIBRARY)
endif()

install(FILES ${HEADERS} DESTINATION include)

# if (NOT(${ROCKY_BINARY_DIR} STREQUAL ${ROCKY_SOURCE_DIR}))
    # install(DIRECTORY ${ROCKY_BINARY_DIR}/include DESTINATION include)
# endif()
