/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    /**
    * System that emits a screen-space frustum grid to the GPU for use with
    * tile-based culling of lights, decals, etc.
    */
    class ROCKY_EXPORT FrustumGridSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, FrustumGridSystemNode>
    {
    public:
        //! Construct the system
        FrustumGridSystemNode(Registry& r);

        void initialize(VSGContext vsgcontext) override;
        void update(VSGContext vsgcontext) override;
        void traverse(vsg::RecordTraversal& record) const override;

    private:
        struct Grid
        {
            ViewIDType viewID;
            vsg::vec4 viewport;
            vsg::mat4 projection;
        };

        struct ViewDetail
        {
            vsg::ref_ptr<vsg::Commands> commands;
            std::optional<Grid> newGrid;
        };

        // Per-view data, calculated during the record traversal
        mutable ViewLocal<ViewDetail> _views;

        vsg::ref_ptr<vsg::ShaderStage> _shader;
        std::shared_ptr<SharedRenderData> _sharedRenderData;

    };
}

EVSG_type_name(rocky::FrustumGridSystemNode)