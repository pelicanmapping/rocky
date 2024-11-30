/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/TerrainTileHost.h>
#include <rocky/Status.h>
#include <rocky/Profile.h>
#include <vsg/nodes/Group.h>

namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Map;
    class TerrainEngine;

    /**
     * Root node of the terrain geometry
     */
    class ROCKY_EXPORT TerrainNode :
        public vsg::Inherit<vsg::Group, TerrainNode>,
        public TerrainSettings,
        public TerrainTileHost
    {
    public:
        //! Construct a new terrain node
        TerrainNode() = default;

        //! Map to render, and profile to render it in
        const Status& setMap(std::shared_ptr<Map> new_map, const Profile& profile, VSGContext& cx);

        //! Clear out the terrain and rebuild it from the map model
        void reset(VSGContext context);

        //! Deserialize from JSON
        Status from_json(const std::string& JSON, const IOOptions& io);

        //! Serialize to JSON
        std::string to_json() const;

        //! Updates the terrain periodically at a safe time.
        //! @return true if any updates were applied
        bool update(const vsg::FrameStamp*, VSGContext& context);

        //! Status of this node; check that's it OK before using
        Status status;

        //! Map containing data model for the terrain
        std::shared_ptr<Map> map;

        //! Engine that renders the terrain
        std::shared_ptr<TerrainEngine> engine;

        //! Terrain's state group
        vsg::ref_ptr<vsg::StateGroup> stategroup;

    protected:

        //! TerrainTileHost interface
        void ping(
            TerrainTileNode* tile,
            const TerrainTileNode* parent,
            vsg::RecordTraversal&) override;

        //! Terrain settings
        const TerrainSettings& settings() override {
            return *this;
        }

    private:

        Status createRootTiles(VSGContext&);

        Profile profile;
        vsg::ref_ptr<vsg::Group> tilesRoot;
    };
}
