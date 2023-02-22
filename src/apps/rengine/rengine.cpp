/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include <rocky_vsg/EngineVSG.h>

#ifdef ROCKY_SUPPORTS_TMS
#include <rocky/TMSImageLayer.h>
#include <rocky/TMSElevationLayer.h>
#endif

#ifdef ROCKY_SUPPORTS_GDAL
#include <rocky/GDALImageLayer.h>
#include <rocky/GDALElevationLayer.h>
#endif

int error(const rocky::Status& status)
{
    rocky::Log::warn() << "Problem with layer: " << status.message << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    rocky::Log::level = rocky::LogLevel::INFO;
    rocky::Log::info() << "Hello, world." << std::endl;

    // instantiate the game engine.
    rocky::EngineVSG engine(argc, argv);

#if defined(ROCKY_SUPPORTS_GDAL)
    auto imagery = rocky::GDALImageLayer::create();
    imagery->setURI("WMTS:https://tiles.maps.eox.at/wmts/1.0.0/WMTSCapabilities.xml,layer=s2cloudless-2020");
    engine.map()->addLayer(imagery);

    if (imagery->status().failed())
        return error(imagery->status());
#endif

#if defined(ROCKY_SUPPORTS_TMS)
    // add an elevation layer
    auto elevation = rocky::TMSElevationLayer::create();
    elevation->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
    engine.map()->addLayer(elevation);

    if (elevation->status().failed())
        return error(elevation->status());
#endif

    // run until the user quits.
    return engine.run();
}