/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/TerrainTileHost.h>
#include <rocky/vsg/terrain/TerrainState.h>
#include <rocky/vsg/terrain/TerrainTilePager.h>
#include <rocky/Result.h>
#include <rocky/Profile.h>
#include <rocky/Layer.h>

namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Map;
    class TerrainEngine;
    class GeoPoint;
    class TerrainNode;

    /**
    * Node that renders a terrain (or part of one) in a specific tiling profile.
    */
    class ROCKY_EXPORT TerrainProfileNode : public vsg::Inherit<vsg::Group, TerrainProfileNode>,
        public TerrainTileHost
    {
    public:
        TerrainProfileNode(const Profile& profile, TerrainNode& terrain);

        //! Terrain housing this profile node
        TerrainNode& terrain;

        //! Tiling profile of this node
        Profile profile;

        //! Rebuilds the profile node contents from scratch.
        void reset(VSGContext context);

        //! Runs periodically to update the terrain tiles if neceessary.
        bool update(VSGContext context);

        //! Access to the engine for stats
        const TerrainEngine& engine() const {
            return *_engine.get();
        }

    public: //! TerrainTileHost interface

        void ping(TerrainTileNode*, const TerrainTileNode*, vsg::RecordTraversal&) override;

        const TerrainSettings& settings() const override;

        TerrainTilePager& tiles() override {
            return _tiles;
        }

    private:

        //! Tracks and updates state for terrain tiles
        TerrainTilePager _tiles;

        std::shared_ptr<TerrainEngine> _engine;

        Result<> createRootTiles(VSGContext);
    };

    /**
     * Root node of the terrain geometry
     */
    class ROCKY_EXPORT TerrainNode : public vsg::Inherit<vsg::StateGroup, TerrainNode>,
        public TerrainSettings
    {
    public:
        
        //! Intersect a point with the loaded terrain geometry.
        Result<GeoPoint> intersect(const GeoPoint& input) const;

    public:
        //! Construct a new terrain node
        TerrainNode(VSGContext);

        //! Map to render, and profile to render it in
        Result<> setMap(
            std::shared_ptr<Map> map,
            const Profile& tilingProfile,
            const SRS& renderingSRS,
            VSGContext cx);

        //! Clear out the terrain and rebuild it from the map model
        void reset(VSGContext context);

        //! Updates the terrain periodically at a safe time.
        //! @return true if any updates were applied
        bool update(VSGContext context);

        //! Map containing data model for the terrain
        std::shared_ptr<const Map> map;

        //! Tiling profile used to subdivide the terrain
        Profile profile;

        //! Spatial reference system of the rendered terrain
        SRS renderingSRS;

        //! Creates Vulkan state for rendering terrain tiles.
        TerrainState terrainState;

        //! Reflects any startup errors that occur
        Status status;

        //! Deserialize from JSON
        Result<> from_json(const std::string& JSON, const IOOptions& io);

        //! Serialize to JSON
        std::string to_json() const;

        //! Get some stats from the terrain engine(s)
        struct Stats {
            size_t numResidentTiles = 0;
            size_t geometryPoolSize = 0;
        };
        Stats stats() const;

    private:

        Result<> createProfiles(VSGContext);
        CallbackSubs _callbacks;
        std::vector<Layer::Ptr> _terrainLayers;
    };
}
