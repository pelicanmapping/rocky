/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <rocky/Horizon.h>
#include <rocky/Map.h>
#include <rocky/Rendering.h>

namespace ROCKY_NAMESPACE
{
    /**
     * VSG Node that renders a map.
     * This node is a "view" component that renders data from a "Map" data model.
     */
    class ROCKY_EXPORT MapNode : public vsg::Inherit<vsg::Group, MapNode>
    {
    public: // constructors

        //! Custom create function
        static vsg::ref_ptr<MapNode> create(VSGContext context);

    public:

        //! Tiling profile for this map node
        Profile profile;

        //! Map backing this map node
        Map::Ptr map;

        //! Node rendering the terrain surface
        vsg::ref_ptr<TerrainNode> terrainNode;

    public:

        //! Spatial reference system of the rendered map.
        const SRS& srs() const;

        //! Mutable access to the terrain settings
        TerrainSettings& terrainSettings();

        //! Immutable access to the terrain settings
        const TerrainSettings& terrainSettings() const;

        //! deserialize from JSON
        Result<> from_json(const std::string& JSON, const IOOptions& io);

        //! Serializes the MapNode
        std::string to_json() const;

        //! Call periodically to update the map node and terrain engine
        //! @return true if updates happened
        bool update(VSGContext context);

    public:

        void traverse(vsg::RecordTraversal&) const override;

        void traverse(vsg::Visitor&) override;

        void traverse(vsg::ConstVisitor&) const override;

    private:

        //! Creates a mapnode with the default global geodetic profile.
        MapNode(VSGContext);

        bool _openedLayers = false;

        mutable ViewLocal<Horizon> _horizon;
    };
}
