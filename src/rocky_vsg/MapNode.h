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
        MapNode(InstanceVSG::ptr instance = {});

        //! Creates a map node that will render the given Map.
        MapNode(shared_ptr<Map> map);

        //! Deserialize a MapNode
        MapNode(const Config& conf, Instance::ptr instance = {});

    public:

        //! Map this node is rendering.
        shared_ptr<Map> map() const;

        //! Screen-space error for geometry level of detail
        void setScreenSpaceError(float sse);
        float screenSpaceError() const;

        //! Whether to allow lighting when present
        void setEnableLighting(const bool& value);
        const bool& getEnableLighting() const;

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

        /**
         * Gets the underlying terrain engine that renders the terrain surface of the map.
         */
        //vsg::ref_ptr<TerrainEngine> getTerrainEngine() const;
        vsg::ref_ptr<TerrainNode> terrainNode() const;

        /**
         * Gets the Config object serializing external data. External data is information
         * that osgEarth itself does not control, but that an app can include in the
         * MapNode for the purposes of including it in a .earth file.
         */
        //Config& externalConfig() { return _externalConf; }
        //const Config& externalConfig() const { return _externalConf; }

        //! Opens all layers that are not already open.
        void openMapLayers();

        //! Serializes the MapNode into a Config object
        Config getConfig() const;

        //! Opens the map (installs a terrain engine and initializes all the layers)
        bool open();

        //! Returns the map coordinates under the provided mouse (window) coordinates.
        //! @param view View in which to do the query
        //! @param mx, my Mouse coordinates
        //! @param out_point Outputs the point under the mouse (when returning true)
        //! @return true upon success, false upon failure
        bool getGeoPointUnderMouse(
            vsg::View* view,
            float mx,
            float my,
            GeoPoint& out_point) const;

        //! Returns the map coordinates under the provided mouse (window) coordinates.
        //! @param view View in which to do the query
        //! @param mx, my Mouse coordinates
        //! @return Outputs the point under the mouse
        GeoPoint getGeoPointUnderMouse(
            vsg::View* view,
            float mx,
            float my) const;

        RuntimeContext& runtime() const;

    public: //override

        //virtual Sphere computeBound() const;

        //virtual void traverse(class osg::NodeVisitor& nv);

        //virtual void resizeGLObjectBuffers(unsigned maxSize);

        //virtual void releaseGLObjects(osg::State* state) const;

        void update(const vsg::FrameStamp*);

    protected:

        virtual ~MapNode();

    private:

        //osg::ref_ptr< MapCallback > _mapCallback;
        //osg::ref_ptr<osg::Uniform> _sseU;
    
        void construct(const Config&);

        //std::shared_ptr<DrapingManager>& getDrapingManager();
        friend class DrapingTechnique;
        friend class DrapeableNode;
        
        //ClampingManager* getClampingManager();
        friend class ClampingTechnique;
        friend class ClampableNode;

        InstanceVSG::ptr _instance;
        optional<bool> _enableLighting;
        optional<bool> _overlayBlending;
        optional<unsigned> _overlayTextureSize;
        optional<bool> _overlayMipMapping;
        optional<float> _overlayResolutionRatio;
        optional<int> _drapingRenderBinNumber;
        optional<float> _screenSpaceError;

        SRS _worldSRS;
        vsg::ref_ptr<TerrainNode> _terrain;
        shared_ptr<Map> _map;
        vsg::ref_ptr<vsg::Group> _layerNodes;
        //unsigned           _lastNumBlacklistedFilenames;
        //vsg::ref_ptr<vsg::Group> _terrainGroup;
        //std::shared_ptr<DrapingManager> _drapingManager;
        //ClampingManager*   _clampingManager;
        std::atomic<bool>  _readyForUpdate;

        bool _isOpen;
    };
}
