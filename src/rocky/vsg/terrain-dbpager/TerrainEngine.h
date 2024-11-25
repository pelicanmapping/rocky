/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/GeometryPool.h>
#include <rocky/vsg/terrain/TerrainState.h>
#include <rocky/vsg/terrain/TerrainTileNode.h>
#include <rocky/vsg/Runtime.h>
#include <rocky/TerrainTileModel.h>

#include <shared_mutex>

namespace ROCKY_NAMESPACE
{
    class Map;

    /**
     * Access to all terrain-specific logic, data, and settings
     * associated with a Map.
     */
    class TerrainEngine
    {
    public:
        TerrainEngine(
            std::shared_ptr<Map> map,
            const SRS& worldSRS,
            Runtime& runtime,
            const TerrainSettings& settings);

        //! Terrain settings.
        const TerrainSettings& settings;

        //! runtime operations (scene graph, views, etc)
        Runtime& runtime;

        //! the map this terrain is rendering
        std::shared_ptr<Map> map;

        //! SRS of the rendered terrain
        SRS worldSRS;

        //! Builds geometry for terrain tiles
        GeometryPool geometryPool;

        //! Creates the state group objects for terrain rendering
        TerrainState stateFactory;

        //! name of job arena used to load data
        std::string loadSchedulerName = "rocky::terrain_loader";

        //! Creates a terrain tile from source
        vsg::ref_ptr<TerrainTileNode> createTile(const TileKey& key, Cancelable& cancelable);

        //! Creates a terrain tile that inherits its data from a parent
        vsg::ref_ptr<TerrainTileNode> inheritTile(const TileKey& key, vsg::ref_ptr<TerrainTileNode> parent, Cancelable& cancelable);

        //! Updates an existing tile with a new data model
        //! @param tile the tile to update
        //! @param cancelable the cancelable object to check for cancellation
        //! @return true if the tile was updated, false if not
        bool updateTile(vsg::ref_ptr<TerrainTileNode> tile, Cancelable& cancelable);

        //! Applies a render model to a tilenew data model to a tile's render model
        void updateRenderModel(TerrainTileNode* tile, const TerrainTileModel& dataModel);
        
        //! Update tick
        bool update(const vsg::FrameStamp* fs, const IOOptions& io);

    private:
        vsg::ref_ptr<vsg::ReaderWriter> _tileLoader;
        mutable std::map<TileKey, vsg::observer_ptr<TerrainTileNode>> _weakTileCache;
        mutable std::shared_mutex _weakTileCache_mutex;
        vsg::ref_ptr<TerrainTileNode> getCachedTile(const TileKey& key) const;
        friend class TerrainTileNodeQuadReader;
    };
}
