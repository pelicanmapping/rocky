/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/Status.h>
#include <rocky/SRS.h>
#include <vsg/nodes/Group.h>

namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Map;
    class SRS;
    class Runtime;
    class TerrainEngine;

    /**
     * Root node of the terrain geometry
     */
    class ROCKY_EXPORT TerrainNode : public vsg::Inherit<vsg::Group, TerrainNode>,
        public TerrainSettings
    {
    public:
        //! Deserialize a new terrain node
        TerrainNode(Runtime& runtime);

        //! Map to render, and SRS to render it in
        const Status& setMap(std::shared_ptr<Map> new_map, const SRS& world_srs);

        //! Clear out the terrain and rebuild it from the map model
        void reset();

        //! Deserialize from JSON
        Status from_json(const std::string& JSON, const IOOptions& io);

        //! Serialize to JSON
        std::string to_json() const;

        //! Updates the terrain periodically at a safe time.
        //! @return true if any updates were applied
        bool update(const vsg::FrameStamp*, const IOOptions& io);

        //! Status of this node; check that's it OK before using
        Status status;

        //! Map containing data model for the terrain
        std::shared_ptr<Map> map;

        //! Engine that renders the terrain
        std::shared_ptr<TerrainEngine> engine;

        //! Terrain settings
        const TerrainSettings& settings() const {
            return *this;
        }

    private:

        void construct();

        Status createRootTiles(const IOOptions& io);

        Runtime& _runtime;
        SRS _worldSRS;
    };
}
