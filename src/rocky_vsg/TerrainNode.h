/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/RuntimeContext.h>
#include <rocky_vsg/TerrainSettings.h>
#include <rocky_vsg/TerrainTileNode.h>
#include <rocky/Config.h>
#include <rocky/Map.h>
#include <vsg/nodes/Group.h>

namespace rocky
{
    class Map;
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

        //! Map to render
        void setMap(shared_ptr<Map> new_map, IOControl* ioc = nullptr);

        //! Serialize
        virtual Config getConfig() const;

        //! Updates the terrain periodically at a safe time
        void update(const vsg::FrameStamp*);

    public: // vsg

        //! Override the Recorder so we can inject information
        void traverse(vsg::RecordTraversal&) const override;

        //! Host callback for tiles to ping
        void ping(
            TerrainTileNode*,
            TerrainTileNode*, 
            TerrainTileNode*, 
            TerrainTileNode*, 
            vsg::RecordTraversal&) override;

    private:

        //! Deserialize and initialize
        void construct(const Config&);

        //! Rebuild the scene graph from the map
        void reloadMap(IOControl* ioc);
        
        RuntimeContext& _runtime;
        vsg::ref_ptr<vsg::Group> _tilesRoot;
        shared_ptr<TerrainContext> _context;
    };
}
