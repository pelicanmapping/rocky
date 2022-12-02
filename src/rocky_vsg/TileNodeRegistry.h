/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/TerrainTileNode.h>
#include <rocky/StringUtils.h>
#include <chrono>

namespace rocky
{
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
        TileNodeRegistry();

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
            TerrainContext& terrain);

        ~TileNodeRegistry();

        //! Adds a tile to the registry. Called by the TileNode itself.
        void add(
            TerrainTileNode* tile,
            TerrainContext& context);

        //! Refresh the tile's tracking info. Called by the TileNode itself
        //! during the cull traversal to let us know it's still active.
        void touch(const TerrainTileNode* tile);

        //! Number of tiles in the registry.
        unsigned size() const { return _tiles.size(); }

        //! Empty the registry, releasing all tiles.
        void releaseAll(); // osg::State* state);

        //! Collect dormant tiles. This is called by UnloaderGroup
        //! during update/event to remove dormant scene graphs.
        void collectDormantTiles(
            std::chrono::steady_clock::time_point olderThanTime,       // collect only if tile is older than this time
            unsigned olderThanFrame,    // collect only if tile is older than this frame
            float fartherThanRange,     // collect only if tile is farther away than this distance (meters)
            unsigned maxCount,          // maximum number of tiles to collect
            TerrainContext& terrain,
            std::vector<vsg::observer_ptr<TerrainTileNode>>& output);   // put dormant tiles here

        //! Update traversal
        void update(vsg::Visitor&);

    protected:

        TileTable _tiles;
        Tracker _tracker;
        mutable util::Mutex _mutex;
        bool _notifyNeighbors;
        //const FrameClock* _clock;

        // for storing neighbor information
        using TileKeySet = std::unordered_set<TileKey>;
        using TileKeyOneToMany = std::unordered_map<TileKey, TileKeySet>;
        TileKeyOneToMany _notifiers;

        // tile nodes requiring an udpate traversal
        std::vector<TileKey> _tilesToUpdate;

    private:

        /** Tells the registry to listen for the TileNode for the specific key
            to arrive, and upon its arrival, notifies the waiter. After notifying
            the waiter, it removes the listen request. (assumes lock held) */
        void startListeningFor(
            const TileKey& keyToWaitFor,
            TerrainTileNode* waiter,
            TerrainContext&);

        /** Removes a listen request set by startListeningFor (assumes lock held) */
        void stopListeningFor(
            const TileKey& keyToWairFor,
            const TileKey& waiterKey,
            TerrainContext&);
    };
}
