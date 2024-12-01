<h1>:mountain: Rocky</h1>

Rocky is a C++ SDK for rendering maps and globes. <img align="right" width="200" alt="Screenshot 2023-02-22 124318" src="https://user-images.githubusercontent.com/326618/220712284-8a17d87a-431f-4966-a425-0f2628b23b40.png">

Rocky will render an accurate 3D or 2D map with real geospatial imagery and elevation data. It supports thousands of map projections and many popular geodata sources including GeoTIFF, TMS, OpenStreetMap, WMTS, WMS, and Azure Maps. Rocky's data model is inspired by the osgEarth SDK, a 3D GIS toolkit created in 2008 and still in wide use today.

This project is in its early stages so expect a lot of API and architectural changes before version 1.0. 

![Windows](https://github.com/pelicanmapping/rocky/actions/workflows/ci-windows-minimal.yml/badge.svg)
![Doxygen](https://github.com/pelicanmapping/rocky/actions/workflows/doxygen-gh-pages.yml/badge.svg)

<!-- TOC start (generated with https://github.com/derlin/bitdowntoc) -->

- [Getting started](#getting-started)
   * [Build the SDK](#build-the-sdk)
      + [Dependencies](#dependencies)
   * [Run the Demo](#run-the-demo)
- [Development](#development)
   * [Hello, world](#hello-world)
      + [main.cpp](#maincpp)
      + [CMakeLists.txt](#cmakeliststxt)
   * [Rocky and Qt](#rocky-and-qt)
   * [Rocky and VulkanSceneGraph](#rocky-and-vulkanscenegraph)
   * [Working with Maps](#working-with-maps)
   * [Drawing Annotations](#drawing-annotations)

<!-- TOC end -->

<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/3767d7e8-364c-498f-a09f-2b0e17b45c0a">

# Getting started

## Build the SDK
Rocky uses CMake, and we maintain the build on Windows and Linux.
Rocky comes with a handy Windows batch file to automatically configure the project using `vcpkg`:
```bat
bootstrap-vcpkg.bat
```
That will download and build all the dependencies (takes a while) and generate your CMake project and Visual Studio solution file.

If you would rather not use vcpkg, you can build and install the [dependencies](#dependencies) yourself, or use your favorite package manager (like `apt` on Linux).

### Dependencies
Thanks to these excellent open source projects!
* [cpp-httplib](https://github.com/yhirose/cpp-httplib)
* [entt](https://github.com/skypjack/entt)
* [GDAL](https://github.com/OSGeo/gdal) (optional)
* [glm](https://github.com/g-truc/glm)
* [ImGui](https://github.com/ocornut/imgui) (optional)
* [nlohmann-json](https://github.com/nlohmann/json)
* [openssl](https://github.com/openssl/openssl) (optional)
* [proj](https://github.com/OSGeo/PROJ)
* [spdlog](https://github.com/gabime/spdlog)
* [sqlite3](https://github.com/sqlite/sqlite) (optional)
* [vsgXchange](https://github.com/vsg-dev/vsgXchange) (optional)
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)
* [weejobs](https://github.com/pelicanmapping/weejobs) (embedded)

## Run the Demo
Rocky is pretty good at finding its data files, but you might need so up a couple environment variables to help:
```bat
set ROCKY_FILE_PATH=%rocky_install_dir%/share/rocky
set ROCKY_DEFAULT_FONT=C:/windows/fonts/arialbd.ttf
set PROJ_DATA=%proj_install_dir%/share/proj
```

If you built with `vcpkg` you will also need to add the dependencies folder to your path; this will normally be found in `vcpkg_installed/x64-windows` (or whatever platform you are using).

Now we're ready:
```
rocky_demo
```
<img width="500" alt="Screenshot 2023-02-22 124318" src="https://user-images.githubusercontent.com/326618/236200807-73567789-a5a3-46d5-a98d-e9c1f24a0f62.png">

There are some JSON map files in the `data` folder. Load one with the `--map` option:
```
rocky_demo --map data\openstreetmap.map.json
```

# Development

## Hello, world
The easiest way to write a turnkey Rocky app is to use the `rocky::Application` object. It will create a viewer, a default map, and a scene graph to store everything you want to visualize.

### main.cpp
```c++
#include <rocky/vsg/Application.h>
#include <rocky/TMSImageLayer.h>

int main(int argc, char** argv)
{
    rocky::Application app(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    return app.run();
}
```

### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.10)
project(myApp VERSION 0.1.0 LANGUAGES CXX C)
find_package(rocky CONFIG REQUIRED)
add_executable(myApp main.cpp)
target_link_libraries(myApp PRIVATE rocky::rocky)
install(TARGETS myApp RUNTIME DESTINATION bin)
```

## Rocky and Qt
You can embed Rocky in a Qt widget. See the `rocky_demo_qt` example for details.

<img width="100%" alt="image" src="https://github.com/user-attachments/assets/84cda604-f617-4562-b208-6d049f8b5ee1">

## Rocky and VulkanSceneGraph
If you're already using VSG in your application and want to add a `MapNode` to a view, do this:
```c++
// Your VSG viewer:
auto viewer = vsg::Viewer::create();

// Make a runtime context for the viewer:
auto context = rocky::VSGContextFactory::create(viewer);

// Make a map node to render your map data:
auto mapNode = rocky::MapNode::create(context);

// optional - add one or more maps to your map:
auto layer = rocky::TMSImageLayer::create();
layer->uri = "https://[abc].tile.openstreetmap.org/{z}/{x}/{y}.png";
layer->setProfile(rocky::Profile::SPHERICAL_MERCATOR);
layer->setAttribution(rocky::Hyperlink{ "\u00a9 OpenStreetMap contributors", "https://openstreetmap.org/copyright" });
mapNode->map->add(layer);
...
scene->addChild(mapNode);
...
// Run your main loop as usual
while (viewer->advanceToNextFrame())
{        
    viewer->handleEvents();
    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();
}
```
You'll probably also want to add the `MapManipulator` to that view to control the map:
```c++
viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, context));
```
## Working with Maps
Coming soon.

## Drawing Annotations
Rocky has a set of building blocks for creating map annotations like labels, icons, and geometry. Rocky uses an ECS (entity component system) based on EnTT to store these primitives. 
Coming soon.

<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/96f128d5-9391-4b92-aa00-6b9fde0a2b35">

