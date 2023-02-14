/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/SurfaceNode.h>
#include <rocky_vsg/TerrainTileHost.h>
#include <rocky/Threading.h>
#include <rocky/TileKey.h>
#include <rocky/Image.h>

#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/app/RecordTraversal.h>
#include <vsg/ui/UIEvent.h> // time_point
#include <vsg/state/Sampler.h>
#include <vsg/state/ImageInfo.h>

namespace ROCKY_NAMESPACE
{
    class CreateTileManifest;
    class SurfaceNode;
    class TerrainTileModel;
    class TerrainTileNode;
    class TerrainContext;
    class TerrainSettings;
    class RuntimeContext;

    struct TextureData
    {
        shared_ptr<Image> image;
        dmat4 matrix{ 1 };
        vsg::ref_ptr<vsg::ImageInfo> texture;
    };

    enum TextureType
    {
        COLOR,
        COLOR_PARENT,
        ELEVATION,
        NORMAL,
        NUM_TEXTURE_TYPES
    };

    struct TerrainTileDescriptors
    {
        struct Uniforms
        {
            fmat4 elevation_matrix;
            fmat4 color_matrix;
            fmat4 normal_matrix;
            fvec2 elev_texel_coeff;
        };
        vsg::ref_ptr<vsg::DescriptorImage> color;
        vsg::ref_ptr<vsg::DescriptorImage> colorParent;
        vsg::ref_ptr<vsg::DescriptorImage> elevation;
        vsg::ref_ptr<vsg::DescriptorImage> normal;
        vsg::ref_ptr<vsg::DescriptorBuffer> uniforms;
        vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSetCommand;
    };

    class TerrainTileRenderModel
    {
    public:
        TextureData color;
        TextureData elevation;
        TextureData normal;
        TextureData colorParent;

        TerrainTileDescriptors descriptorModel;

        void applyScaleBias(const dmat4& sb)
        {
            if (color.image)
                color.matrix *= sb;
            if (elevation.image)
                elevation.matrix *= sb;
            if (normal.image)
                normal.matrix *= sb;
            if (colorParent.image)
                colorParent.matrix *= sb;
        }
    };

    /**
     * TileNode represents a single tile. TileNode has 5 children:
     * one SurfaceNode that renders the actual tile content under a MatrixTransform;
     * and four TileNodes representing the LOD+1 quadtree tiles under this tile.
     */
    class ROCKY_VSG_EXPORT TerrainTileNode : public vsg::Inherit<vsg::CullGroup, TerrainTileNode>
    {
    public:
        TileKey key;
        bool doNotExpire;
        Revision revision;
        fvec2 morphConstants;
        float childrenVisibilityRange;
        unsigned numLODs;
        TerrainTileRenderModel renderModel;
        
        //vsg::ref_ptr<TerrainTileNode> parent;
        //vsg::observer_ptr<TerrainTileNode> parent;
        vsg::ref_ptr<SurfaceNode> surface;
        vsg::ref_ptr<vsg::StateGroup> stategroup;
        
        //mutable util::Future<vsg::ref_ptr<vsg::Group>> childrenCreator;
        //mutable util::Future<bool> childrenMerger;
        mutable util::Future<bool> childrenLoader;
        mutable util::Future<TerrainTileModel> dataLoader;
        mutable util::Future<bool> dataMerger;
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
            const SRS& worldSRS,
            const TerrainTileDescriptors& initialDescriptorModel,
            TerrainTileHost* in_host,
            RuntimeContext& runtime);

        virtual ~TerrainTileNode();

        //! Tells this tile to load all its layers
        void refreshAllLayers();

        //! Tells this tile to request data for the data in the manifest
        void refreshLayers(const CreateTileManifest& manifest);

        /** Notifies this tile that another tile has come into existence. */
        void notifyOfArrival(
            TerrainTileNode* that,
            shared_ptr<TerrainContext> terrain);

        /** Returns the tile's parent; convenience function */
        //inline vsg::ref_ptr<TerrainTileNode> parentTile() const {
        //    return parent.ref_ptr();
        //}

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

        //! Apply any thread-safe updates to the tile
        void update(
            const vsg::FrameStamp* fs,
            const IOOptions& io);

        //! Remove this tile's children and reset the child
        //! loader future.
        void unloadChildren();

    public:

        //! Customized cull traversal
        void accept(vsg::RecordTraversal& visitor) const override;
        
    protected:

        mutable bool _needsChildren;
        mutable bool _needsUpdate;
        TerrainTileHost* _host;
        vsg::observer_ptr<TerrainTileNode> _eastNeighbor;
        vsg::observer_ptr<TerrainTileNode> _southNeighbor;
        fvec4 _tileKeyValue;

        void inheritFrom(vsg::ref_ptr<TerrainTileNode> parent);

        void updateElevationRaster();

    private:

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
