/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/InstanceVSG.h>
#include <rocky/Map.h>
#include <vsg/nodes/Group.h>
#include <vsg/app/CompileManager.h>

namespace vsg {
    class Viewer;
}

namespace ROCKY_NAMESPACE
{
    class SRS;
    class DrapableNode;
    class ClampableNode;
    class TerrainNode;
    class SceneGraphContext;

    namespace util
    {
        class DrapingManager;
        class ClampingManager;
        class DrapingTechnique;
        class ClampingTechnique;
    }

    /**
     * OSG Node that forms the root of an osgEarth map.
     * This node is a "view" component that renders data from a "Map" data model.
     */
    class ROCKY_VSG_EXPORT MapNode : public vsg::Inherit<vsg::Group, MapNode>
    {
    public: // constructors

        //! Creates an empty map node (with a default empty Map)
        MapNode(const InstanceVSG& instance);

        //! Creates a map node that will render the given Map.
        MapNode(shared_ptr<Map> map);

        //! Deserialize a MapNode
        explicit MapNode(const JSON& conf, const InstanceVSG& instance);

    public:

        //! Map this node is rendering.
        shared_ptr<Map> map() const;

        //! Screen-space error for geometry level of detail
        void setScreenSpaceError(float sse);
        float screenSpaceError() const;

        //! Spatial Reference System of the underlying map.
        const SRS& mapSRS() const;

        //! Spatial reference system of the rendered map.
        const SRS& worldSRS() const;

        /**
         * Finds the topmost Map node in the specified scene graph, or returns NULL if
         * no Map node exists in the graph.
         *
         * @param graph
         *      Node graph in which to search for a MapNode
         * @param travMask
         *      Traversal mask to apply while searching
         */
        static shared_ptr<MapNode> get(
            const vsg::Node* graph,
            unsigned travMask = ~0);

        /**
         * Accesses the group node that contains all the nodes added by Layers.
         */
        vsg::ref_ptr<vsg::Group> getLayerNodeGroup() const;

        //! Gets the underlying terrain engine that renders the terrain surface of the map.
        vsg::ref_ptr<TerrainNode> terrainNode() const;

        //! Opens all layers that are not already open.
        void openMapLayers();

        //! Serializes the MapNode into a Config object
        JSON to_json() const;

        //! Opens the map (installs a terrain engine and initializes all the layers)
        bool open();

        //! Runtime tools
        RuntimeContext& runtime();

        void update(const vsg::FrameStamp*);

        virtual ~MapNode();

    public:

        void accept(vsg::RecordTraversal&) const override;

    private:

        void construct(const JSON&);

        InstanceVSG _instance;

        optional<float> _screenSpaceError = 25.0f;

        SRS _worldSRS;
        vsg::ref_ptr<TerrainNode> _terrain;
        shared_ptr<Map> _map;
        vsg::ref_ptr<vsg::Group> _layerNodes;
        std::atomic<bool> _readyForUpdate;

        bool _isOpen;
    };
}
