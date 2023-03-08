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
set(HEADERS
    Callbacks.h
    Color.h
    Common.h
    Config.h
    DateTime.h
    ElevationLayer.h
    # ElevationPool.h
    Ellipsoid.h
    GeoCircle.h
    GeoCommon.h
    GeoExtent.h
    GeoHeightfield.h
    Geoid.h
    GeoImage.h
    GeoPoint.h
    Heightfield.h
    Horizon.h
    Image.h
    ImageLayer.h
    ImageMosaic.h
    Instance.h
    IOTypes.h
    Layer.h
    LayerCollection.h
    LayerCollection.inline.h
    LayerReference.h
    Log.h
    Map.h
    Math.h
    Metrics.h
    optional.h
    Profile.h
    rtree.h
    sha1.h
    SRS.h
    Status.h
    TerrainResolver.h
    TerrainTileModel.h
    TerrainTileModelFactory.h
    Threading.h
    TileKey.h
    TileLayer.h
    Units.h
    URI.h
    Utils.h
    Viewpoint.h
    VisibleLayer.h
    weemesh.h
    ${version_h}
)

set(SOURCES    
    Color.cpp
    Config.cpp
    DateTime.cpp
    ElevationLayer.cpp
    # ElevationPool.cpp
    Ellipsoid.cpp
    GeoCircle.cpp
    GeoExtent.cpp
    GeoHeightfield.cpp
    Geoid.cpp
    GeoImage.cpp
    GeoPoint.cpp
    Heightfield.cpp
    Horizon.cpp
    Image.cpp
    ImageLayer.cpp
    ImageMosaic.cpp
    Instance.cpp
    IOTypes.cpp
    Layer.cpp
    LayerCollection.cpp
    Log.cpp
    Map.cpp
    Math.cpp
    Metrics.cpp
    Profile.cpp
    SRS.cpp
    Status.cpp
    TerrainTileModel.cpp
    TerrainTileModelFactory.cpp
    Threading.cpp
    TileKey.cpp
    TileLayer.cpp
    Units.cpp
    URI.cpp
    Utils.cpp
    Viewpoint.cpp
    VisibleLayer.cpp
)

if (ROCKY_SUPPORTS_PROFILING)
    list(APPEND HEADERS Profiling.h)
endif()
        
if(ROCKY_SUPPORTS_GDAL)
    list(APPEND HEADERS GDAL.h   GDALImageLayer.h   GDALElevationLayer.h)
    list(APPEND SOURCES GDAL.cpp GDALImageLayer.cpp GDALElevationLayer.cpp)
endif()

if(ROCKY_SUPPORTS_TMS)
    list(APPEND HEADERS TMS.h   TMSImageLayer.h   TMSElevationLayer.h)
    list(APPEND SOURCES TMS.cpp TMSImageLayer.cpp TMSElevationLayer.cpp)
endif()

# set up library dependencies
set(PUBLIC_LIBS glm::glm)

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
if (SQLITE3_FOUND)
    list(APPEND PRIVATE_LIBS unofficial::sqlite3::sqlite3)
    #include_directories(${SQLITE3_INCLUDE_DIR})
    #list(APPEND PRIVATE_LIBS ${SQLITE3_LIBRARY})target_link_libraries(main PRIVATE unofficial::sqlite3::sqlite3)
endif()

set(LIBRARIES PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})


add_library(rocky ${HEADERS} ${SOURCES})

set_target_properties(rocky PROPERTIES FOLDER "rocky")

if(MSVC)
    target_compile_options(rocky PRIVATE "/MP")
endif()

# enable folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

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
