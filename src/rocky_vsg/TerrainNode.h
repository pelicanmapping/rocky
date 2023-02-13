/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TerrainTileHost.h>
#include <rocky/Config.h>
#include <vsg/nodes/Group.h>

namespace ROCKY_NAMESPACE
{
    class IOOptions;
    class Map;
    class RuntimeContext;
    class SRS;
    class TerrainContext;

    /**
     * Root node of the terrain geometry
     */
    class ROCKY_VSG_EXPORT TerrainNode :
        public vsg::Inherit<vsg::Group, TerrainNode>,
        public TerrainSettings,
        public TerrainTileHost
    {
    public:
        //! Deserialize a new terrain node
        TerrainNode(
            RuntimeContext& runtime,
            const Config& conf);

        //! Map to render, and SRS to render it in
        Status setMap(
            shared_ptr<Map> new_map,
            const SRS& world_srs);

        //! Serialize
        virtual Config getConfig() const;

        //! Updates the terrain periodically at a safe time
        void update(const vsg::FrameStamp*, const IOOptions& io);

    protected:

        //! TerrainTileHost interface
        void ping(
            TerrainTileNode*,
            TerrainTileNode*, 
            TerrainTileNode*, 
            TerrainTileNode*, 
            vsg::RecordTraversal&) override;

        //! Access to terrain settings
        const TerrainSettings& settings() override {
            return *this;
        }

    private:

        //! Deserialize and initialize
        void construct(const Config&);
        
        RuntimeContext& _runtime;
        vsg::ref_ptr<vsg::Group> _tilesRoot;
        shared_ptr<TerrainContext> _context;
    };
}
