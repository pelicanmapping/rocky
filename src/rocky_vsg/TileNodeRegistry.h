/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/GeometryPool.h>
#include <rocky_vsg/SelectionInfo.h>
#include <rocky_vsg/StateFactory.h>
#include <rocky_vsg/TerrainTileNode.h>

//#include <rocky/Utils.h>
#include <chrono>

namespace ROCKY_NAMESPACE
{
    class TerrainSettings;
    class RuntimeContext;

    /**
     * Keeps track of all the tiles resident in the terrain engine.
     */
    class TileNodeRegistry
    {
    public:
        using Ptr = std::shared_ptr<TileNodeRegistry>;

        using Tracker = util::SentryTracker<TerrainTileNode*>;

        struct TableEntry
        {
            // this needs to be a ref ptr because it's possible for the unloader
            // to remove a Tile's ancestor from the scene graph, which will turn
            // this Tile into an orphan. As an orphan it will expire and eventually
            // be removed anyway, but we need to keep it alive in the meantime...
            vsg::ref_ptr<TerrainTileNode> _tile;
            void* _trackerToken;
            TableEntry() : _trackerToken(nullptr) { }
        };

        using TileTable = std::unordered_map<TileKey, TableEntry>;

    public:
        TileNodeRegistry(TerrainTileHost* host);

        shared_ptr<GeometryPool> geometryPool;
        shared_ptr<StateFactory> stateFactory;
        shared_ptr<SelectionInfo> selectionInfo;

        ~TileNodeRegistry();

        //! TerrainTileNode will call this to let us know that it's alive
        //! and that it may need something.
        //! ONLY call during record.
        void ping(
            TerrainTileNode* tile,
            const TerrainTileNode* parent,
            vsg::RecordTraversal&);

        //! Number of tiles in the registry.
        unsigned size() const { return _tiles.size(); }

        //! Empty the registry, releasing all tiles.
        void releaseAll();

        //! Update traversal
        void update(
            const vsg::FrameStamp* fs,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain);

        //! Create a single terrain tile.
        vsg::ref_ptr<TerrainTileNode> createTile(
            const TileKey& key,
            vsg::ref_ptr<TerrainTileNode> parent,
            shared_ptr<TerrainContext> terrain);

        //! Fetches a tile by its key.
        //! @param key TileKey for which to fetch a tile
        //! @return The tile, if it exists
        vsg::ref_ptr<TerrainTileNode> getTile(const TileKey& key) const;

    protected:

        TileTable _tiles;
        Tracker _tracker;
        mutable std::mutex _mutex;
        TerrainTileHost* _host;

        std::vector<TileKey> _loadChildren;
        std::vector<TileKey> _loadElevation;
        std::vector<TileKey> _mergeElevation;
        std::vector<TileKey> _loadData;
        std::vector<TileKey> _mergeData; 
        std::vector<TileKey> _updateData;

    private:

        void requestLoadChildren(
            vsg::ref_ptr<TerrainTileNode> parent,
            shared_ptr<TerrainContext> terrain) const;

        void requestLoadElevation(
            vsg::ref_ptr<TerrainTileNode> tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;

        void requestMergeElevation(
            vsg::ref_ptr<TerrainTileNode> tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;

        void requestLoadData(
            vsg::ref_ptr<TerrainTileNode> tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;

        void requestMergeData(
            vsg::ref_ptr<TerrainTileNode> tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;
    };
}
