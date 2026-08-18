/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/SRS.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/TextureResource.h>
#include <array>

namespace ROCKY_NAMESPACE
{
    struct AutoOverlayTransform
    {
    };

    //! Compatibility name for the output of an Overlay bake. New code should
    //! consume TextureResource instead.
    using OverlayBakeTexture = TextureResource;

    class ROCKY_EXPORT OverlayBakeSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, OverlayBakeSystemNode>
    {
    public:
        OverlayBakeSystemNode(Registry& registry);

        //! System collection from which render-to-texture capabilities are discovered.
        //! This is rescanned during update so extensions can register live.
        ECSNode* renderSourceSystems = nullptr;
        vsg::ref_ptr<vsg::Group> bakeScene;
        SRS worldSRS;
        unsigned textureSize = 512u;

        //! Maximum number of new offscreen render targets to create and
        //! synchronously compile in one frame. Zero means unlimited.
        unsigned maxResourceSetupsPerFrame = 2u;

        // Advanced tuning factor for auto-computed overlay depth thickness.
        // 1.0 = default behavior; >1.0 = safer/thicker; <1.0 = tighter/faster.
        float depthSafetyFactor = 1.0f;

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    private:
        enum class BakePhase
        {
            WaitingForSources,
            Priming,
            Baking,
            Ready
        };

        struct OverlayBakeDetail : public Component<OverlayBakeDetail>
        {
            vsg::ref_ptr<vsg::ImageInfo> texture;
            vsg::ref_ptr<vsg::RenderGraph> renderGraph;
            vsg::ref_ptr<vsg::Node> viewNode;
            vsg::ref_ptr<vsg::CommandGraph> hostCommandGraph;
            glm::uvec2 textureSize = { 0u, 0u };
            bool useDepthBuffer = false;
            std::size_t boundsRevision = 0u;
            std::size_t contentRevision = 0u;
            bool boundsRevisionValid = false;
            bool contentRevisionValid = false;
            bool autoTransformDirty = true;
            bool fitToSources = true;
            bool published = false;
            bool generationPending = true;
            BakePhase phase = BakePhase::WaitingForSources;
        };

        // Color-only and depth-enabled bakes need separate view IDs so VSG can
        // compile compatible graphics-pipeline implementations for each render pass.
        std::array<vsg::ref_ptr<vsg::Camera>, 2> _sharedCameras;
        std::array<vsg::ref_ptr<vsg::View>, 2> _sharedViews;
        std::array<vsg::ref_ptr<vsg::RenderPass>, 2> _sharedRenderPasses;
        vsg::ref_ptr<vsg::Context> _resourceContext;
        std::vector<RenderTextureParticipant*> _renderParticipants;
        mutable float _lastDepthSafetyFactor = -1.0f;
        SRS _lastWorldSRS;
        void refreshRenderParticipants();

        void on_construct_Overlay(entt::registry& r, entt::entity e);
        void on_destroy_Overlay(entt::registry& r, entt::entity e);
        void on_update_Overlay(entt::registry& r, entt::entity e);
        void on_construct_RenderTexture(entt::registry& r, entt::entity e);
        void on_destroy_RenderTexture(entt::registry& r, entt::entity e);
        void on_update_RenderTexture(entt::registry& r, entt::entity e);
        void on_destroy_OverlayBakeDetail(entt::registry& r, entt::entity e);

        bool createBakeResources(
            VSGContext vsgcontext,
            const glm::uvec2& textureSize,
            bool useDepthBuffer,
            vsg::ref_ptr<vsg::ImageInfo>& outTexture,
            vsg::ref_ptr<vsg::RenderGraph>& outRenderGraph,
            vsg::ref_ptr<vsg::Node>& outViewNode,
            vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph);
        bool updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail, bool recomputeAutoTransform) const;
    };
}

EVSG_type_name(rocky::OverlayBakeSystemNode)
