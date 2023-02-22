# :mountain: Rocky

Rocky is a 3D geospatial map & terrain rendering engine.

Rocky will render an accurate 3D or 2D map with real geospatial imagery and elevation data. It supports thousands of map projections (thanks to the *PROJ* toolkit) and most popular geodata formats including GeoTIFF, WMTS, WMS, and TMS.

This project is in its early stages. It is based on the data model in the osgEarth SDK, a 3D GIS toolkit created in 2008 and still in wide use today.

# A simple Rocky application

### main.cpp
```c++
#include <rocky_vsg/EngineVSG.h>
#include <rocky/TMSImageLayer.h>

int main(int argc, char** argv)
{
    rocky::EngineVSG engine(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->setURI("https://readymap.org/readymap/tiles/1.0.0/7/");
    engine.map()->addLayer(imagery);

    if (imagery->status().failed()) 
    {
        rocky::Log::warn() << imagery->status().message << std::endl;
        return -1;
    }

    return engine.run();
}
```

### CMakeLists.txt
```
cmake_minimum_required(VERSION 3.10)

project(myApp VERSION 0.1.0 LANGUAGES CXX C)

find_package(myApp CONFIG REQUIRED)

add_executable(myApp main.cpp)

target_link_libraries(myApp PRIVATE rocky::rocky rocky::rocky_vsg)

install(TARGETS myApp RUNTIME DESTINATION bin)
```

### Dependencies
Rocky requires the following additional dependencies:

* glm
* PROJ
* vsg (VulkanSceneGraph)
* GDAL (optional)
* vsgXchange (optional)

You can install these using your favorite package manager (we recommend `vcpkg`) or you can build them yourself.

### Environment variables

Set up a couple env vars so Rocky can find its data.
```
set ROCKY_FILE_PATH=%rocky_install_dir%/share/shaders

set PROJ_DATA=%proj_install_dir%/share/proj
```
