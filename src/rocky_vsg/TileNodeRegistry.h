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

        //! Marks all tiles intersecting the extent as dirty. If incremental
        //! update is enabled, they will automatically reload.
        //! NOTE: Input extent SRS must match the terrain's SRS exactly.
        //!       The method does not check.
        void setDirty(
            const GeoExtent& extent,
            unsigned minLevel,
            unsigned maxLevel,
            const CreateTileManifest& manifest,
            shared_ptr<TerrainContext> terrain);

        ~TileNodeRegistry();

        //! TerrainTileNode will call this to let us know that it's alive
        //! and that it may need something.
        //! ONLY call during record.
        void ping(
            TerrainTileNode* tile0,
            TerrainTileNode* tile1,
            TerrainTileNode* tile2,
            TerrainTileNode* tile3,
            vsg::RecordTraversal& nv); // override;

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
            TerrainTileNode* parent,
            shared_ptr<TerrainContext> terrain);

        vsg::ref_ptr<TerrainTileNode> getTile(const TileKey& key) const;

    protected:

        TileTable _tiles;
        Tracker _tracker;
        mutable std::mutex _mutex;
        TerrainTileHost* _host;

        std::vector<TileKey> _needsChildren;
        std::vector<TileKey> _needsLoad;
        std::vector<TileKey> _needsMerge;
        std::vector<TileKey> _needsUpdate;

    private:

        void createTileChildren(
            TerrainTileNode* parent,
            shared_ptr<TerrainContext> terrain) const;

        void requestLoad(
            TerrainTileNode* tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;

        void requestMerge(
            TerrainTileNode* tile,
            const IOOptions& io,
            shared_ptr<TerrainContext> terrain) const;
    };
}
