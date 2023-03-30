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

template<class T>
int error(T layer)
{
    rocky::Log::warn() << "Problem with layer \"" <<
        layer->name() << "\" : " << layer->status().message << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    // instantiate the game engine.
    rocky::EngineVSG engine(argc, argv);

#if defined(ROCKY_SUPPORTS_TMS)

    // add a layer to the map
    auto layer = rocky::TMSImageLayer::create();
    layer->setURI("https://readymap.org/readymap/tiles/1.0.0/135/");
    engine.map()->layers().add(layer);

    if (layer->status().failed())
        return error(layer);

    // add an elevation layer
    auto elevation = rocky::TMSElevationLayer::create();
    elevation->setEncoding(rocky::ElevationLayer::Encoding::MapboxRGB);
    elevation->setURI("https://readymap.org/readymap/tiles/1.0.0/116/");
    engine.map()->layers().add(elevation);

    if (elevation->status().failed())
        return error(elevation);
#endif

    // run until the user quits.
    return engine.run();
}
