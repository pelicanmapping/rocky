/**
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Decal.h>
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/TextureResource.h>

namespace ROCKY_NAMESPACE
{
    /**
     * ECS system that handles Decal components
     */
    class ROCKY_EXPORT DecalSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, DecalSystemNode>
    {
    public:
        //! Construct the system
        DecalSystemNode(Registry& registry);

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    public: // vsg::Object
        void traverse(vsg::RecordTraversal&) const override;
        void traverse(vsg::ConstVisitor&) const override;
        void traverse(vsg::Visitor&) override;

    private:

        // Per-view data, calculated during the record traversal
        struct ViewDetail
        {
            vsg::ref_ptr<vsg::Commands> commands;
        };
        mutable ViewLocal<ViewDetail> _views;
        mutable std::uint64_t _lastFrameCount = ~0U;
        std::shared_ptr<SharedRenderData> _sharedRenderData;

        // collection of all decals in the scene
        mutable unsigned _totalNumDecals = 0u;

        vsg::ref_ptr<vsg::ShaderStage> _cullingShader;

        // Descriptor slots whose owning style component was removed. They are
        // released during update(), where the shared descriptor arena is available.
        std::mutex _pendingTextureSlotsMutex;
        std::vector<std::int32_t> _pendingTextureSlots;

        void rebuildCommands(ViewIDType, VSGContext);
        void updateStyles(VSGContext);
        void resizeGPUBuffersIfNeeded(VSGContext);
        void updateDecalsSSBO(VSGContext);

        void on_construct_Decal(entt::registry& r, entt::entity e);
        void on_construct_DecalStyle(entt::registry& r, entt::entity e);
        void on_construct_Overlay(entt::registry& r, entt::entity e);
        void on_construct_ProjectedTexture(entt::registry& r, entt::entity e);
        void on_construct_TextureResource(entt::registry& r, entt::entity e);
        void on_destroy_Decal(entt::registry& r, entt::entity e);
        void on_destroy_DecalStyle(entt::registry& r, entt::entity e);
        void on_destroy_Overlay(entt::registry& r, entt::entity e);
        void on_destroy_ProjectedTexture(entt::registry& r, entt::entity e);
        void on_destroy_TextureResource(entt::registry& r, entt::entity e);
        void on_destroy_TextureSlotDetail(entt::registry& r, entt::entity e);
        void on_update_Decal(entt::registry& r, entt::entity e);
        void on_update_DecalStyle(entt::registry& r, entt::entity e);
        void on_update_Overlay(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::DecalSystemNode)
