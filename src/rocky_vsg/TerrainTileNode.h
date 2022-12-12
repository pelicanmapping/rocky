/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/SurfaceNode.h>
#include <rocky_vsg/TileRenderModel.h>
#include <rocky/TileKey.h>

#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/ui/UIEvent.h> // time_point

namespace rocky
{
    class CreateTileManifest;
    class SurfaceNode;
    class TerrainTileModel;
    class TerrainTileNode;
    class TerrainContext;
    class TerrainSettings;

    /**
     * Interface for a tile to notify the engine that
     * certain tiles are active in the scene graph.
     */
    class TerrainTileHost
    {
    public:
        virtual void ping(
            TerrainTileNode* tile0,
            TerrainTileNode* tile1,
            TerrainTileNode* tile2,
            TerrainTileNode* tile3,
            vsg::RecordTraversal& nv) = 0;
    };

    /**
     * TileNode represents a single tile. TileNode has 5 children:
     * one SurfaceNode that renders the actual tile content under a MatrixTransform;
     * and four TileNodes representing the LOD+1 quadtree tiles under this tile.
     */
    class ROCKY_VSG_EXPORT TerrainTileNode :
        public vsg::Inherit<vsg::CullGroup, TerrainTileNode>
    {
    public:
        TileKey key;
        bool doNotExpire;
        Revision revision;
        fvec2 morphConstants;
        float childrenVisibilityRange;
        unsigned numLODs;
        TileRenderModel renderModel;
        
        vsg::observer_ptr<TerrainTileNode> parent;
        vsg::ref_ptr<SurfaceNode> surface;
        vsg::ref_ptr<vsg::StateGroup> stategroup;
        
        mutable util::Future<bool> childLoader;
        mutable std::atomic<uint64_t> lastTraversalFrame;
        mutable std::atomic<vsg::time_point> lastTraversalTime;
        mutable std::atomic<float> lastTraversalRange;

        //! Construct a new tile node
        TerrainTileNode(
            const TileKey& key,
            TerrainTileNode* parent,
            vsg::ref_ptr<vsg::Node> geometry,
            const fvec2& morphConstants,
            float childrenVisibilityRange,
            const TileDescriptorModel& initialDescriptorModel,
            TerrainTileHost* in_host);

        virtual ~TerrainTileNode();

        //! Tells this tile to load all its layers
        void refreshAllLayers();

        //! Tells this tile to request data for the data in the manifest
        void refreshLayers(const CreateTileManifest& manifest);

        //! Install new geometry in this tile
        //void createGeometry(
        //    TerrainContext& terrain,
        //    Cancelable*);

        /** Initial inheritance of tile data from the parent node. */
        //void initializeData(TerrainContext& terrain);

#if 0
        /** Whether the tile is expired; i.e. has not been visited in some time. */
        bool isDormant() const;

        /** Whether all the subtiles are this tile are dormant (have not been visited recently) */
        bool areSubTilesDormant() const;

        /** Whether all 3 quadtree siblings of this tile are dormant */
        bool areSiblingsDormant() const;
#endif

        /** Notifies this tile that another tile has come into existence. */
        void notifyOfArrival(
            TerrainTileNode* that,
            shared_ptr<TerrainContext> terrain);

        /** Returns the tile's parent; convenience function */
        inline vsg::ref_ptr<TerrainTileNode> getParentTile() const {
            return vsg::ref_ptr<TerrainTileNode>(parent);
        }

        /** Elevation data for this node along with its scale/bias matrix; needed for bounding box */
        void setElevation(
            shared_ptr<Image> image,
            const dmat4& matrix);

        shared_ptr<Image> getElevationRaster() const {
            return surface->getElevationRaster();
        }

        const dmat4& getElevationMatrix() const {
            return surface->getElevationMatrix();
        }

        // access to subtiles
        inline TerrainTileNode* subTile(unsigned i) const
        {            
            return children.size() < 2 ? nullptr :
                static_cast<TerrainTileNode*>(
                    static_cast<vsg::Group*>(children[1].get())->children[i].get());
        }

        //! Merge new Tile model data into this tile
        void merge(
            const TerrainTileModel& model,
            const CreateTileManifest& manifest,
            shared_ptr<TerrainContext> terrain);

        //! TODO?
        void loadSync(shared_ptr<TerrainContext> terrain);

        //! Apply any thread-safe updates to the tile
        void update(const vsg::FrameStamp* fs);

        //! Remove this tile's children and reset the child
        //! loader future.
        void unloadChildren();

    public:

        //TODO: implement for a custom cull traversal...
        void accept(vsg::RecordTraversal& visitor) const override;

        //void resizeGLObjectBuffers(unsigned maxSize);

        //void releaseGLObjects(osg::State* state) const;

    protected:

        mutable fvec4 _tileKeyValue;
        bool _needsData;
        mutable bool _needsChildren;
        bool _needsUpdate;
        TerrainTileHost* _host;
        vsg::observer_ptr<TerrainTileNode> _eastNeighbor;
        vsg::observer_ptr<TerrainTileNode> _southNeighbor;

        void inherit();

        void updateElevationRaster();

    private:

        //void refreshStateGroup(
        //    TerrainContext&);

        void updateNormalMap(const TerrainSettings&);

        bool shouldSubDivide(vsg::State* state) const;

        // whether this tile should render the given pass
        //bool passInLegalRange(const RenderingPass&) const;

        /** Load (or continue loading) content for the tiles in this quad. */
        //void load(
        //    vsg::State* state,
        //    TerrainContext& terrain) const;

        /** Ensure that inherited data from the parent node is up to date. */
        void refreshInheritedData(const TerrainTileNode* parent);

        // Inherit one shared sampler from parent tile if possible
        void inheritSharedSampler(int binding);

        //! Calculate the culling extent
        void recomputeBound();

        //! Whether child tiles are present
        inline bool hasChildren() const {
            return children.size() >= 2;
        }

        friend class TileNodeRegistry;
    };
}
