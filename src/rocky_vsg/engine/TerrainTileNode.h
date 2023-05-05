/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky_vsg/Common.h>
#include <rocky_vsg/engine/SurfaceNode.h>
#include <rocky_vsg/engine/TerrainTileHost.h>
#include <rocky/Threading.h>
#include <rocky/TileKey.h>
#include <rocky/Image.h>
#include <rocky/TerrainTileModel.h>

#include <vsg/nodes/QuadGroup.h>
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
    class TerrainTileNode;
    class TerrainContext;
    class TerrainSettings;
    class Runtime;

    struct TextureData
    {
        shared_ptr<Image> image;
        glm::dmat4 matrix{ 1 };
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
            glm::fmat4 elevation_matrix;
            glm::fmat4 color_matrix;
            glm::fmat4 normal_matrix;
            glm::fmat4 model_matrix;
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
        glm::fmat4 modelMatrix;
        TextureData color;
        TextureData elevation;
        TextureData normal;
        TextureData colorParent;

        TerrainTileDescriptors descriptors;

        void applyScaleBias(const glm::dmat4& sb)
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
        glm::fvec2 morphConstants;
        float childrenVisibilityRange;
        unsigned numLODs;
        TerrainTileRenderModel renderModel;
        
        vsg::ref_ptr<SurfaceNode> surface;
        vsg::ref_ptr<vsg::StateGroup> stategroup;
        
        mutable util::Future<bool> childrenLoader;
        mutable util::Future<TerrainTileModel> elevationLoader;
        mutable util::Future<bool> elevationMerger;
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
            const glm::fvec2& morphConstants,
            float childrenVisibilityRange,
            const SRS& worldSRS,
            const TerrainTileDescriptors& initialDescriptors,
            TerrainTileHost* in_host,
            Runtime& runtime);

        //! Elevation data for this node along with its scale/bias matrix;
        //! needed for bounding box
        void setElevation(
            shared_ptr<Image> image,
            const glm::dmat4& matrix);

        //! This node's elevation raster image
        shared_ptr<Image> getElevationRaster() const {
            return surface->getElevationRaster();
        }

        //! This node's elevation matrix
        const glm::dmat4& getElevationMatrix() const {
            return surface->getElevationMatrix();
        }

        //! Remove this tile's children and reset the child
        //! loader future.
        void unloadChildren();

        //! Update this node (placeholder)
        void update(const vsg::FrameStamp*, const IOOptions&) { }

    public:

        //! Customized cull traversal
        void accept(vsg::RecordTraversal& visitor) const override;
        
    protected:

        mutable bool _needsChildren;
        mutable bool _needsUpdate;
        TerrainTileHost* _host;

        // set the tile's render model equal to the specified parent's
        // render model, and then apply a scale bias matrix so it
        // inherits the textures.
        void inheritFrom(vsg::ref_ptr<TerrainTileNode> parent);

    private:

        bool shouldSubDivide(vsg::State* state) const;

        //! Calculate the culling extent
        void recomputeBound();

        //! Whether child tiles are present
        inline bool hasChildren() const {
            return children.size() >= 2;
        }

        //! access to subtiles. Make sure they exist before calling this.
        inline TerrainTileNode* subTile(unsigned i) const
        {
            return static_cast<TerrainTileNode*>(
                static_cast<vsg::QuadGroup*>(children[1].get())->children[i].get());
        }

        friend class TerrainTilePager;
    };
}
