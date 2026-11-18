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

    /**
     * Keeps track of all the tiles resident in the terrain engine.
     */
    class TerrainTilePager
    {
    public:
        using Ptr = std::shared_ptr<TerrainTilePager>;

        using Tracker = detail::SentryTracker<TerrainTileNode*>;

        //! Owns a compiled candidate until publication or cancellation; retirement is deferred through VSG.
        struct VectorPublication
        {
            VSGContext context;
            vsg::ref_ptr<vsg::BindDescriptorSet> base;
            std::optional<TerrainTileRenderModel> model;
            //! Even abandoned worker results must retire compiled descriptors safely.
            ~VectorPublication() { if (model) context->dispose(model->descriptors.bind); }
        };

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
            std::optional<std::pair<TileKey, std::size_t>> failedVectorPublication;
            std::optional<std::pair<TileKey, std::size_t>> vectorRequest;
            jobs::future<std::shared_ptr<VectorPublication>> vectorPreparation;
            jobs::future<bool> vectorMerger;
        };

        using TileTable = std::map<TileKey, TileInfo>;

    public:
        //! Consturct the tile manager.
        TerrainTilePager(const TerrainSettings& settings, TerrainTileHost* host, VSGContext vsgcontext);

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
        bool update(std::shared_ptr<TerrainTileFactory> tileFactory, VSGContext vsgcontext);

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

        std::vector<TileKey> _createChildren;
        std::vector<TileKey> _loadData;
        std::vector<TileKey> _mergeData;
        std::vector<TileKey> _updateData;

        unsigned _firstLOD = 0u;
        CallbackSubscriptions _subs;
        Revision _sharedDataRevision = 0;

    private:

        struct VectorLoad
        {
            jobs::future<Result<vsg::ref_ptr<TerrainVectorTexture>>> future;
            std::uint64_t lastUsed = 0;
            // Dynamic priority shared with queued terrain-loader jobs; does not own a tile.
            std::shared_ptr<std::atomic<float>> priority = std::make_shared<std::atomic<float>>(-FLT_MAX);
            bool reportedFailure = false;
        };
        // One request per source ancestor/revision, shared by all overzoomed children.
        std::map<std::pair<TileKey, std::size_t>, VectorLoad> _vectorLoads;

        std::vector<TileKey> _vectorDemand;
        std::uint64_t _vectorDemandFrame = 0;

        //! Consumes terrain visibility demand using the terrain loader and metered merge queue.
        //! Vectors run alongside imagery and never gate progressive terrain refinement.
        bool updateVectors(std::shared_ptr<TerrainTileFactory>, VSGContext);

        //! Loads the geometry for 4 new subtiles, and inherits their data models from a parent.
        void requestCreateChildren(
            TileInfo& info,
            std::shared_ptr<TerrainTileFactory> terrain,
            VSGContext vsgcontext) const;

        //! Loads new data for a tile that was prepped in loadSubtiles
        void requestLoadData(
            TileInfo& info,
            const IOOptions& io,
            std::shared_ptr<TerrainTileFactory> terrain,
            VSGContext vsgcontext) const;

        //! Merges the new data model loaded in loadData.
        void requestMergeData(
            TileInfo& info,
            const IOOptions& io,
            std::shared_ptr<TerrainTileFactory> terrain,
            VSGContext vsgcontext) const;


        void recompileDescriptorSets(VSGContext vsgcontext);
    };
}
