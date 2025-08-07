/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainTileNode.h>
#include <rocky/SentryTracker.h>
#include <chrono>
#include <map>

namespace ROCKY_NAMESPACE
{
    class TerrainSettings;
    class Runtime;

    /**
     * Keeps track of all the tiles resident in the terrain engine.
     */
    class TerrainTilePager
    {
    public:
        using Ptr = std::shared_ptr<TerrainTilePager>;

        using Tracker = util::SentryTracker<TerrainTileNode*>;

        struct TileInfo
        {
            // this needs to be a ref ptr because it's possible for the unloader
            // to remove a Tile's ancestor from the scene graph, which will turn
            // this Tile into an orphan. As an orphan it will expire and eventually
            // be removed anyway, but we need to keep it alive in the meantime...
            vsg::ref_ptr<TerrainTileNode> tile;
            void* trackerToken = nullptr;
            jobs::future<vsg::ref_ptr<vsg::Node>> childrenCreator;
            jobs::future<bool> dataLoader;
            jobs::future<bool> dataMerger;
        };

        using TileTable = std::map<TileKey, TileInfo>;

    public:
        //! Consturct the tile manager.
        TerrainTilePager(const TerrainSettings& settings, TerrainTileHost* host);

        ~TerrainTilePager();

        //! TerrainTileNode will call this to let us know that it's alive
        //! and that it may need something.
        //! ONLY call during record.
        void ping(
            TerrainTileNode* tile,
            const TerrainTileNode* parent,
            vsg::RecordTraversal&);

        //! Number of tiles in the registry.
        std::size_t size() const { return _tiles.size(); }

        //! Empty the registry, releasing all tiles.
        void releaseAll();

        //! Update traversal.
        //! @return true if any changes occurred.
        bool update(
            const vsg::FrameStamp* fs,
            const IOOptions& io,
            std::shared_ptr<TerrainEngine> engine);

        //! Fetches a tile by its key.
        //! @param key TileKey for which to fetch a tile
        //! @return The tile, if it exists
        vsg::ref_ptr<TerrainTileNode> getTile(const TileKey& key) const;

        TileTable _tiles;
        Tracker _tracker;
        std::uint64_t _lastUpdate = 0;
        mutable std::mutex _mutex;
        TerrainTileHost* _host;
        const TerrainSettings& _settings;
        bool _updateViewerRequired = false;

        std::vector<TileKey> _createChildren;
        std::vector<TileKey> _loadData;
        std::vector<TileKey> _mergeData;
        std::vector<TileKey> _updateData;

        unsigned _firstLOD = 0u;

    private:

        //! Loads the geometry for 4 new subtiles, and inherits their data models from a parent.
        void requestCreateChildren(
            TileInfo& info,
            std::shared_ptr<TerrainEngine> terrain) const;

        //! Loads new data for a tile that was prepped in loadSubtiles
        void requestLoadData(
            TileInfo& info,
            const IOOptions& io,
            std::shared_ptr<TerrainEngine> terrain) const;

        //! Merges the new data model loaded in loadData.
        void requestMergeData(
            TileInfo& info,
            const IOOptions& io,
            std::shared_ptr<TerrainEngine> terrain) const;
    };
}
