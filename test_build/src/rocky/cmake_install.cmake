# Install script for directory: /home/runner/work/rocky/rocky/src/rocky

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky/rocky-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky/rocky-targets.cmake"
         "/home/runner/work/rocky/rocky/test_build/src/rocky/CMakeFiles/Export/765d96849f963fdd41fc35c41750e2d4/rocky-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky/rocky-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky/rocky-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky" TYPE FILE FILES "/home/runner/work/rocky/rocky/test_build/src/rocky/CMakeFiles/Export/765d96849f963fdd41fc35c41750e2d4/rocky-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky" TYPE FILE FILES "/home/runner/work/rocky/rocky/test_build/src/rocky/CMakeFiles/Export/765d96849f963fdd41fc35c41750e2d4/rocky-targets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rocky" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/test_build/src/rocky/rocky-config.cmake"
    "/home/runner/work/rocky/rocky/test_build/src/rocky/rocky-config-version.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/runner/work/rocky/rocky/test_build/src/rocky/librocky.so.27")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librocky.so.27")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/runner/work/rocky/rocky/test_build/src/rocky/librocky.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/Azure.h"
    "/home/runner/work/rocky/rocky/src/rocky/AzureImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/Bing.h"
    "/home/runner/work/rocky/rocky/src/rocky/BingElevationLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/BingImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/Cache.h"
    "/home/runner/work/rocky/rocky/src/rocky/Callbacks.h"
    "/home/runner/work/rocky/rocky/src/rocky/Color.h"
    "/home/runner/work/rocky/rocky/src/rocky/Common.h"
    "/home/runner/work/rocky/rocky/src/rocky/Context.h"
    "/home/runner/work/rocky/rocky/src/rocky/DateTime.h"
    "/home/runner/work/rocky/rocky/src/rocky/ECS.h"
    "/home/runner/work/rocky/rocky/src/rocky/ElevationLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/ElevationSampler.h"
    "/home/runner/work/rocky/rocky/src/rocky/Ellipsoid.h"
    "/home/runner/work/rocky/rocky/src/rocky/Ephemeris.h"
    "/home/runner/work/rocky/rocky/src/rocky/Feature.h"
    "/home/runner/work/rocky/rocky/src/rocky/GDAL.h"
    "/home/runner/work/rocky/rocky/src/rocky/GDALElevationLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/GDALFeatureSource.h"
    "/home/runner/work/rocky/rocky/src/rocky/GDALImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/GeoCircle.h"
    "/home/runner/work/rocky/rocky/src/rocky/GeoExtent.h"
    "/home/runner/work/rocky/rocky/src/rocky/GeoImage.h"
    "/home/runner/work/rocky/rocky/src/rocky/GeoPoint.h"
    "/home/runner/work/rocky/rocky/src/rocky/Geocoder.h"
    "/home/runner/work/rocky/rocky/src/rocky/Heightfield.h"
    "/home/runner/work/rocky/rocky/src/rocky/Horizon.h"
    "/home/runner/work/rocky/rocky/src/rocky/IOTypes.h"
    "/home/runner/work/rocky/rocky/src/rocky/Image.h"
    "/home/runner/work/rocky/rocky/src/rocky/ImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/Layer.h"
    "/home/runner/work/rocky/rocky/src/rocky/Log.h"
    "/home/runner/work/rocky/rocky/src/rocky/MBTiles.h"
    "/home/runner/work/rocky/rocky/src/rocky/MBTilesElevationLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/MBTilesImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/Map.h"
    "/home/runner/work/rocky/rocky/src/rocky/Math.h"
    "/home/runner/work/rocky/rocky/src/rocky/Memory.h"
    "/home/runner/work/rocky/rocky/src/rocky/Profile.h"
    "/home/runner/work/rocky/rocky/src/rocky/Rendering.h"
    "/home/runner/work/rocky/rocky/src/rocky/Result.h"
    "/home/runner/work/rocky/rocky/src/rocky/SRS.h"
    "/home/runner/work/rocky/rocky/src/rocky/SentryTracker.h"
    "/home/runner/work/rocky/rocky/src/rocky/TMS.h"
    "/home/runner/work/rocky/rocky/src/rocky/TMSElevationLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/TMSImageLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/TerrainTileModel.h"
    "/home/runner/work/rocky/rocky/src/rocky/TerrainTileModelFactory.h"
    "/home/runner/work/rocky/rocky/src/rocky/Threading.h"
    "/home/runner/work/rocky/rocky/src/rocky/TileKey.h"
    "/home/runner/work/rocky/rocky/src/rocky/TileLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/URI.h"
    "/home/runner/work/rocky/rocky/src/rocky/Units.h"
    "/home/runner/work/rocky/rocky/src/rocky/Utils.h"
    "/home/runner/work/rocky/rocky/src/rocky/Viewpoint.h"
    "/home/runner/work/rocky/rocky/src/rocky/VisibleLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/json.h"
    "/home/runner/work/rocky/rocky/src/rocky/option.h"
    "/home/runner/work/rocky/rocky/src/rocky/rocky.h"
    "/home/runner/work/rocky/rocky/src/rocky/rocky_imgui.h"
    "/home/runner/work/rocky/rocky/src/rocky/rtree.h"
    "/home/runner/work/rocky/rocky/src/rocky/weejobs.h"
    "/home/runner/work/rocky/rocky/src/rocky/weemesh.h"
    "/home/runner/work/rocky/rocky/test_build/build_include/rocky/Version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky/ecs" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Component.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Declutter.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/EntityCollectionLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Icon.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Label.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Line.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Mesh.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Motion.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Registry.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Transform.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Visibility.h"
    "/home/runner/work/rocky/rocky/src/rocky/ecs/Widget.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky/contrib" TYPE FILE FILES "/home/runner/work/rocky/rocky/src/rocky/contrib/EarthFileImporter.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/rocky/data" TYPE FILE FILES "/home/runner/work/rocky/rocky/data/fonts/times.vsgb")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky/vsg" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/vsg/Application.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/Common.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/DisplayManager.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/GeoTransform.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/MapManipulator.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/MapNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/NodeLayer.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/NodePager.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/PipelineState.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/PixelScaleTransform.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/RTT.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/SkyNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/VSGContext.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/VSGUtils.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/json.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky/vsg/terrain" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/GeometryPool.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/SurfaceNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainEngine.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainSettings.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainState.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainTileHost.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainTileNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/terrain/TerrainTilePager.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rocky/vsg/ecs" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/DeclutterSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/ECSNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/EntityNode.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/FeatureView.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/IconSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/IconSystem2.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/LabelSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/LineSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/MeshSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/MotionSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/NodeGraph.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/System.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/TransformDetail.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/TransformSystem.h"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/ecs/WidgetSystem.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/rocky/shaders" TYPE FILE FILES
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.atmo.ground.vert.glsl"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.atmo.sky.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.atmo.sky.vert"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.icon.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.icon.indirect.cull.comp"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.icon.indirect.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.icon.indirect.vert"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.icon.vert"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.lighting.frag.glsl"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.line.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.line.vert"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.mesh.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.mesh.vert"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.terrain.frag"
    "/home/runner/work/rocky/rocky/src/rocky/vsg/shaders/rocky.terrain.vert"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/rocky/rocky/test_build/src/rocky/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
