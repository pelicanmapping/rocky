/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky_vsg/Merger.h>
#include <rocky_vsg/RuntimeContext.h>
#include <rocky_vsg/SelectionInfo.h>
#include <rocky_vsg/StateFactory.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TileNodeRegistry.h>
#include <rocky_vsg/TileRenderModel.h>
//#include <rocky_vsg/Unloader.h>

//#include <vsg/io/Options.h>
//#include <vsg/utils/ShaderSet.h>
//#include <vsg/utils/SharedObjects.h>

namespace rocky
{
    class Map;

    class TerrainContext : public TerrainSettings
    {
    public:
        TerrainContext(
            RuntimeContext& runtime,
            const Config& conf);

        //! runtime operations (scene graph, views, etc)
        RuntimeContext& runtime;

        //! the map this terrain is rendering
        shared_ptr<Map> map;

        //! creator of terrain til geometry
        GeometryPool geometryPool;

        //! merges new tiles into the live scene graph
        Merger merger;

        //! tracks all existing tiles
        TileNodeRegistry tiles;

        //! manages visibility and morphing ranges
        internal::SelectionInfo selectionInfo;

        //! Facotry for creating rendering state commands
        StateFactory stateFactory;

        //! name of job arena used to load data
        std::string loadArenaName = "terrain.load";

    private:

        vsg::ref_ptr<vsg::ShaderSet> createTerrainShaderSet() const;
    };

}
