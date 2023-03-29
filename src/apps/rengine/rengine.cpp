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

#ifdef ROCKY_SUPPORTS_MBTILES
#include <rocky/MBTilesImageLayer.h>
#include <rocky/MBTilesElevationLayer.h>
#endif

template<class T>
int error(T layer)
{
    rocky::Log::warn() << "Problem with layer \"" <<
        layer->name() << "\" : " << layer->status().message << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    rocky::Log::level = rocky::LogLevel::INFO;
    rocky::Log::info() << "Hello, world." << std::endl;

    // instantiate the game engine.
    rocky::EngineVSG engine(argc, argv);

#if 0 //defined(ROCKY_SUPPORTS_MBTILES)

    auto imagery = rocky::MBTilesImageLayer::create();
    imagery->setName("mbtiles test layer");
    imagery->setURI("D:/data/simdis/readymap.mbtiles");
    engine.map()->layers().add(imagery);

    if (imagery->status().failed())
        return error(imagery);

#elif defined(ROCKY_SUPPORTS_GDAL)
    auto imagery = rocky::GDALImageLayer::create();
    imagery->setURI("WMTS:https://tiles.maps.eox.at/wmts/1.0.0/WMTSCapabilities.xml,layer=s2cloudless-2020_3857");
    engine.map()->layers().add(imagery);

    if (imagery->status().failed())
        return error(imagery);

    auto elevation = rocky::TMSElevationLayer::create();
    elevation->setTileSize(256);
    elevation->setEncoding(rocky::ElevationLayer::Encoding::MapboxRGB);
    elevation->setProfile(rocky::Profile(
        rocky::SRS::SPHERICAL_MERCATOR,
        rocky::Box(-20037508.34, -20037508.34, 20037508.34, 20037508.34),
        1, 1));
    elevation->setURI("https://maps2.anduril.com/tiled/tileset/bathymetry-tcarta-hae/tile?x=${x}&y=${y}&z=${z}");
    engine.map()->layers().add(elevation);

    if (elevation->status().failed())
        return error(elevation);

#elif defined(ROCKY_SUPPORTS_TMS)

    // add a layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->setURI("https://readymap.org/readymap/tiles/1.0.0/135/");
    engine.map()->layers().add(layer);

    if (layer->status().failed())
        return error(layer);

    // add an elevation layer
    auto elevation = rocky::TMSElevationLayer::create();
    elevation->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
    engine.map()->layers().add(elevation);

    if (elevation->status().failed())
        return error(elevation);
#endif

    // run until the user quits.
    return engine.run();
}
