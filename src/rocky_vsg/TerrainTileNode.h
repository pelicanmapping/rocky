/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/LoadTileData.h>
#include <rocky_vsg/SurfaceNode.h>
#include <rocky_vsg/TileRenderModel.h>

#include <rocky/Heightfield.h>
#include <rocky/IOTypes.h>
#include <rocky/TerrainTileModel.h>
#include <rocky/TileKey.h>

#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/app/CompileManager.h>
#include <vsg/ui/UIEvent.h> // time_point

namespace rocky
{
    class EngineContext;
    class LoadTileOperation;
    class SurfaceNode;
    class SelectionInfo;
    class TerrainCuller;
    class TerrainSettings;

    /**
     * TileNode represents a single tile. TileNode has 5 children:
     * one SurfaceNode that renders the actual tile content under a MatrixTransform;
     * and four TileNodes representing the LOD+1 quadtree tiles under this tile.
     */
    class ROCKY_VSG_EXPORT TerrainTileNode :
        public vsg::Inherit<vsg::CullGroup, TerrainTileNode>
    {
    public:
        TerrainTileNode(
            const TileKey& key,
            TerrainTileNode* parent,
            TerrainContext& terrain,
            Cancelable* ioc);

        virtual ~TerrainTileNode();

        /** TileKey of the key representing the data in this node. */
        const TileKey& getKey() const { return _key; }

        /** Indicates that this tile should never be unloaded. */
        void setDoNotExpire(bool value);

        bool getDoNotExpire() const { return _doNotExpire; }

#if 1
        /** Tells this tile to load all its layers. */
        void refreshAllLayers();

        /** Tells this tile to request data for the data in the manifest */
        void refreshLayers(
            const CreateTileManifest& manifest);
#endif

        //! Install new geometry in this tile
        void createGeometry(
            TerrainContext& terrain,
            Cancelable*);

        /** Initial inheritance of tile data from the parent node. */
        void initializeData(TerrainContext& terrain);

        /** Whether the tile is expired; i.e. has not been visited in some time. */
        bool isDormant() const;

        /** Whether all the subtiles are this tile are dormant (have not been visited recently) */
        bool areSubTilesDormant() const;

        /** Whether all 3 quadtree siblings of this tile are dormant */
        bool areSiblingsDormant() const;

        /** Removed any sub tiles from the scene graph. Please call from a safe thread only (update) */
        void removeSubTiles();

        /** Notifies this tile that another tile has come into existence. */
        void notifyOfArrival(
            TerrainTileNode* that,
            TerrainContext&);

        /** Returns the tile's parent; convenience function */
        inline vsg::ref_ptr<TerrainTileNode> getParentTile() const {
            return vsg::ref_ptr<TerrainTileNode>(_parentTile);
        }

        //! Node containing the surface state + geometry
        SurfaceNode* surface() const {
            return static_cast<SurfaceNode*>(children[0].get());
        }

        vsg::StateGroup* stateGroup() const {
            return static_cast<vsg::StateGroup*>(surface()->children[0].get());
        }

        /** Elevation data for this node along with its scale/bias matrix; needed for bounding box */
        void setElevation(
            shared_ptr<Image> image,
            const dmat4& matrix);

        shared_ptr<Image> getElevationRaster() const {
            return surface()->getElevationRaster();
        }

        const dmat4& getElevationMatrix() const {
            return surface()->getElevationMatrix();
        }

        // access to subtiles
        inline TerrainTileNode* subTile(unsigned i) const
        {            
            return children.size() < 2 ? nullptr :
                static_cast<TerrainTileNode*>(
                    static_cast<vsg::Group*>(children[1].get())->children[i].get());
        }

        /** Merge new Tile model data into this tile's rendering data model. */
        void merge(
            const TerrainTileModel& model,
            const CreateTileManifest& manifest,
            TerrainContext& terrain);

        /** Access the rendering model for this tile */
        TileRenderModel& renderModel() { return _renderModel; }
        const TileRenderModel& renderModel() const { return _renderModel; }

        const fvec4& getTileKeyValue() const { return _tileKeyValue; }

        const fvec2& getMorphConstants() const { return _morphConstants; }

        void loadSync(TerrainContext&);

        //void refreshSharedSamplers(const RenderBindings& bindings);

        int getRevision() const { return _revision; }

        bool isEmpty() const { return _empty; }

        float getLoadPriority() const { return _loadPriority; }

        // whether the TileNodeRegistry should update-traverse this node
        bool updateRequired() const {
            return _imageUpdatesActive;
        }

        // update-traverse this node, updating any images that require
        // and update-traverse
        void update(const vsg::FrameStamp* fs);

        int getLastTraversalFrame() const {
            return _lastTraversalFrame;
        }

        std::chrono::steady_clock::time_point getLastTraversalTime() const {
            return _lastTraversalTime;
        }

        float getLastTraversalRange() const {
            return _lastTraversalRange;
        }

        void resetTraversalRange() {
            _lastTraversalRange = FLT_MAX;
        }

    public:

        //TODO: implement for a custom cull traversal...
        void accept(vsg::RecordTraversal& visitor) const override;

    public: 

        //void recomputeBound();

        //void traverse(osg::NodeVisitor& nv);


        //void resizeGLObjectBuffers(unsigned maxSize);

        //void releaseGLObjects(osg::State* state) const;

    protected:

        TileKey _key;
        vsg::observer_ptr<TerrainTileNode> _parentTile;
        //vsg::ref_ptr<SurfaceNode> _surface;
        //vsg::ref_ptr<vsg::StateGroup> _stateGroup;
        mutable util::Mutex _mutex;

        mutable std::atomic<uint64_t> _lastTraversalFrame;
        mutable std::atomic<vsg::time_point> _lastTraversalTime;
        mutable std::atomic<float> _lastTraversalRange;

        mutable fvec4 _tileKeyValue;
        float _childrenVisibilityRange;
        unsigned _numLODs;
        fvec2 _morphConstants;
        TileRenderModel _renderModel;
        bool _empty;
        bool  _imageUpdatesActive;
        TileKey _subdivideTestKey;
        bool _doNotExpire;
        int _revision;
        bool _createChildAsync;
        mutable std::atomic<float> _loadPriority;

        using CreateChildResult = vsg::ref_ptr<TerrainTileNode>;
        std::vector<util::Future<CreateChildResult>> _createChildResults;

        using LoadQueue = std::queue<LoadTileDataOperation::Ptr>;
        mutable util::Mutexed<LoadQueue> _loadQueue;
        mutable unsigned _loadsInQueue;
        mutable const CreateTileManifest* _nextLoadManifestPtr;

        bool dirty() const {
            return _loadsInQueue > 0;
        }

//        bool nextLoadIsProgressive() const;

        vsg::observer_ptr<TerrainTileNode> _eastNeighbor;
        vsg::observer_ptr<TerrainTileNode> _southNeighbor;

        void inherit(TerrainContext&);

        mutable util::Future<bool> _childLoader;

        void updateElevationRaster();

    private:

        void refreshStateGroup(
            TerrainContext&);

        void updateNormalMap(
            const TerrainSettings&);

        void createChildren(
            TerrainContext&) const;

        //vsg::ref_ptr<TerrainTileNode> createChild(
        //    const TileKey& key,
        //    Cancelable* cancelable);

        // Returns false if the Surface node fails visiblity test
        bool cull(vsg::RecordTraversal& nv) const;

        //bool cull_spy(TerrainCuller*);

        bool shouldSubDivide(vsg::State* state) const;

        // whether this tile should render the given pass
        //bool passInLegalRange(const RenderingPass&) const;

        /** Load (or continue loading) content for the tiles in this quad. */
        void load(
            vsg::State* state,
            TerrainContext& terrain) const;

        /** Ensure that inherited data from the parent node is up to date. */
        void refreshInheritedData(const TerrainTileNode* parent);

        // Inherit one shared sampler from parent tile if possible
        void inheritSharedSampler(int binding);

        //void rebuildStateGroup(
        //    TerrainContext& );

        void recomputeBound();

        vsg::ref_ptr<vsg::StateGroup> createStateGroup(
            TerrainContext&) const;

        inline bool hasChildren() const {
            return children.size() >= 2;
        }
    };
}
