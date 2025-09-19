/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/vsg/terrain/TerrainState.h>
#include <rocky/vsg/terrain/SurfaceNode.h>
#include <rocky/Threading.h>
#include <rocky/TileKey.h>

namespace ROCKY_NAMESPACE
{
    class CreateTileManifest;
    class TerrainEngine;
    class TerrainSettings;
    class TerrainTileHost;

    /**
     * TileNode represents a single tile. TileNode has 5 children:
     * one SurfaceNode that renders the actual tile content under a MatrixTransform;
     * and four TileNodes representing the LOD+1 quadtree tiles under this tile.
     */
    class ROCKY_EXPORT TerrainTileNode : public vsg::Inherit<vsg::CullGroup, TerrainTileNode>
    {
    public:
        TileKey key;
        bool doNotExpire = false;
        Revision revision = 0;
        TerrainTileRenderModel renderModel;       
        vsg::ref_ptr<SurfaceNode> surface;
        vsg::ref_ptr<vsg::StateGroup> stategroup;
        
        mutable jobs::future<bool> subtilesLoader;
        mutable std::atomic<uint64_t> lastTraversalFrame = { 0 };
        mutable std::atomic<vsg::time_point> lastTraversalTime;
        mutable std::atomic<float> lastTraversalRange = { FLT_MAX };

        //! Update this node (placeholder).
        //! @return true if any changes occur.
        bool update(const vsg::FrameStamp*, const IOOptions&) { return false; }

    public:
        //! Customized cull traversal
        void accept(vsg::RecordTraversal& visitor) const override;

        //! Intersectors, etc.
        void accept(vsg::ConstVisitor& visitor) const override;
        
    protected:

        mutable bool needsSubtiles = false;
        mutable bool needsUpdate = false;
        TerrainTileHost* host = nullptr;

        // set the tile's render model equal to the specified parent's
        // render model, and then apply a scale bias matrix so it
        // inherits the textures.
        void inheritFrom(vsg::ref_ptr<TerrainTileNode> parent);

    private:

        //! Whether child tiles are present
        inline bool subtilesExist() const
        {
            return children.size() >= 2;
        }

        //! access to subtiles. Make sure they exist before calling this.
        inline TerrainTileNode* subTile(unsigned i) const
        {
            return static_cast<TerrainTileNode*>(
                static_cast<vsg::QuadGroup*>(children[1].get())->children[i].get());
        }

        friend class TerrainEngine;
        friend class TerrainTilePager;
    };
}
