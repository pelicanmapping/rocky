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

//#include <rocky/StringUtils.h>
#include <chrono>

namespace rocky
{
    class TerrainSettings;
    class RuntimeContext;

    /**
     * Keeps track of all the tiles resident in the terrain engine.
     */
    class TileNodeRegistry : public TerrainTileHost
    {
    public:
        using Ptr = std::shared_ptr<TileNodeRegistry>;

        using Tracker = util::SentryTracker<TerrainTileNode*>;
        //using Tracker = util::SentryTracker<TerrainTileQuad*>;

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

        //! Sets the frame clock to use
        //void setFrameClock(const FrameClock* value) { _clock = value; }

        //! Whether tiles will listen for their neighbors to arrive in order to
        //! facilitate normal map edge matching.
        void setNotifyNeighbors(bool value);

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

        //! Adds a tile to the registry. Called by the TileNode itself.
        void add(
            TerrainTileNode* tile,
            shared_ptr<TerrainContext> terrain);

        //! TerrainTileNode will call this to let us know that it's alive
        //! and that it may need something.
        //! ONLY call suring record.
        void ping(
            TerrainTileNode* tile0,
            TerrainTileNode* tile1,
            TerrainTileNode* tile2,
            TerrainTileNode* tile3,
            vsg::RecordTraversal& nv) override;

        //void ping(const TerrainTileQuad* quad);

        //! Number of tiles in the registry.
        unsigned size() const { return _tiles.size(); }

        //! Empty the registry, releasing all tiles.
        void releaseAll();

        //! Update traversal
        void update(
            const vsg::FrameStamp* fs,
            shared_ptr<TerrainContext> terrain);

        //! Create a single terrain tile.
        vsg::ref_ptr<TerrainTileNode> createTile(
            const TileKey& key,
            TerrainTileNode* parent,
            shared_ptr<TerrainContext> terrain);

    protected:

        TileTable _tiles;
        Tracker _tracker;
        mutable util::Mutex _mutex;
        bool _notifyNeighbors;
        //const FrameClock* _clock;
        TerrainTileHost* _host;

        // for storing neighbor information
        using TileKeySet = std::unordered_set<TileKey>;
        using TileKeyOneToMany = std::unordered_map<TileKey, TileKeySet>;
        TileKeyOneToMany _notifiers;

        std::vector<TileKey> _needsUpdate;
        std::vector<TileKey> _needsData;
        std::vector<TileKey> _needsChildren;

    private:

        /** Tells the registry to listen for the TileNode for the specific key
            to arrive, and upon its arrival, notifies the waiter. After notifying
            the waiter, it removes the listen request. (assumes lock held) */
        void startListeningFor(
            const TileKey& keyToWaitFor,
            TerrainTileNode* waiter,
            shared_ptr<TerrainContext>);

        /** Removes a listen request set by startListeningFor (assumes lock held) */
        void stopListeningFor(
            const TileKey& keyToWairFor,
            const TileKey& waiterKey,
            shared_ptr<TerrainContext>);

#if 0
        //! Collect dormant tiles. This is called by UnloaderGroup
        //! during update/event to remove dormant scene graphs.
        void collectDormantTiles(
            const TerrainSettings&,
            const vsg::FrameStamp*);
#endif

        //std::chrono::steady_clock::time_point olderThanTime,       // collect only if tile is older than this time
        //unsigned olderThanFrame,    // collect only if tile is older than this frame
        //float fartherThanRange,     // collect only if tile is farther away than this distance (meters)
        //unsigned maxCount,          // maximum number of tiles to collect
        //TerrainContext& terrain,
        //std::vector<vsg::observer_ptr<TerrainTileNode>>& output);   // put dormant tiles here

        //vsg::ref_ptr<TerrainTileNode> createTile(
        //    const TileKey& key,
        //    TerrainTileNode* parent,
        //    const TerrainSettings& settings,
        //    shared_ptr<TerrainTileHost> host);

        void createTileChildren(
            TerrainTileNode* parent,
            shared_ptr<TerrainContext> terrain) const;

        void requestTileData(
            TerrainTileNode* tile,
            shared_ptr<TerrainContext> terrain) const;
    };
}
