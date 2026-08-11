/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Overlay.h>
#include <rocky/SRS.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    struct AutoOverlayTransform
    {
    };

    struct OverlayBakeTexture : public Component<OverlayBakeTexture>
    {
        vsg::ref_ptr<vsg::ImageInfo> texture;
    };

    class ROCKY_EXPORT OverlayBakeSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, OverlayBakeSystemNode>
    {
    public:
        OverlayBakeSystemNode(Registry& registry);

        vsg::ref_ptr<vsg::Node> bakeScene;
        SRS worldSRS;
        unsigned textureSize = 512u;

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    private:
        struct OverlayBakeDetail : public Component<OverlayBakeDetail>
        {
            vsg::ref_ptr<vsg::ImageInfo> texture;
            vsg::ref_ptr<vsg::RenderGraph> renderGraph;
            vsg::ref_ptr<vsg::Node> viewNode;
            vsg::ref_ptr<vsg::CommandGraph> hostCommandGraph;
            glm::uvec2 textureSize = { 0u, 0u };
            entt::entity styleEntity = entt::null;
        };

        vsg::ref_ptr<vsg::Camera> _sharedCamera;
        vsg::ref_ptr<vsg::View> _sharedView;

        void on_construct_Overlay(entt::registry& r, entt::entity e);
        void on_destroy_Overlay(entt::registry& r, entt::entity e);
        void on_update_Overlay(entt::registry& r, entt::entity e);
        void on_destroy_OverlayBakeDetail(entt::registry& r, entt::entity e);

        bool createBakeResources(
            VSGContext vsgcontext,
            const glm::uvec2& textureSize,
            vsg::ref_ptr<vsg::ImageInfo>& outTexture,
            vsg::ref_ptr<vsg::RenderGraph>& outRenderGraph,
            vsg::ref_ptr<vsg::Node>& outViewNode,
            vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph);
        bool updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail) const;
    };
}

EVSG_type_name(rocky::OverlayBakeSystemNode)
