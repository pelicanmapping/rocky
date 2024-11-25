/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/InstanceVSG.h>
#include <rocky/vsg/terrain/TerrainSettings.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <rocky/vsg/ViewLocal.h>
#include <rocky/Map.h>
#include <rocky/Horizon.h>
#include <vsg/nodes/Group.h>

namespace ROCKY_NAMESPACE
{
    class SRS;

    /**
     * VSG Node that renders a map.
     * This node is a "view" component that renders data from a "Map" data model.
     */
    class ROCKY_EXPORT MapNode : public vsg::Inherit<vsg::Group, MapNode>
    {
    public: // constructors

        //! Creates an empty mapnode with a default Rocky instance.
        MapNode();

        //! Creates an empty map node (with a default empty Map)
        MapNode(const InstanceVSG& instance);

        //! Creates a map node that will render the given Map.
        MapNode(std::shared_ptr<Map> map);

        //! Deserialize a MapNode
        explicit MapNode(const JSON& conf, const InstanceVSG& instance);

    public:

        //! Map backing this map node
        std::shared_ptr<Map> map;

        //! Instance object assocated with this map node
        InstanceVSG instance;

        //! Node rendering the terrain surface
        vsg::ref_ptr<TerrainNode> terrainNode;

    public:

        //! Spatial Reference System of the underlying map.
        const SRS& mapSRS() const;

        //! Spatial reference system of the rendered map.
        const SRS& worldSRS() const;

        //! Mutable access to the terrain settings
        TerrainSettings& terrainSettings();

        //! Immutable access to the terrain settings
        const TerrainSettings& terrainSettings() const;

        //! deserialize from JSON
        Status from_json(const std::string& JSON, const IOOptions& io);

        //! Serializes the MapNode
        std::string to_json() const;

        //! Call periodically to update the map node and terrain engine
        //! @return true if updates happened
        bool update(const vsg::FrameStamp*);

    public:

        void accept(vsg::RecordTraversal&) const override;

    private:

        void construct();

        SRS _worldSRS;
        vsg::ref_ptr<vsg::Group> _layerNodes;
        bool _openedLayers = false;

        struct ViewLocalData {
            std::shared_ptr<Horizon> horizon;
        };
        mutable util::ViewLocal<ViewLocalData> _viewlocal;
    };
}
