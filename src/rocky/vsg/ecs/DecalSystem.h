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
     * Normalizes decal facades and publishes projected payloads to the GPU.
     *
     * Decal, DecalStyle, and Overlay are adapted into the lower-level
     * ProjectedTexture and texture-source components. The system then assigns
     * visible TextureResource or SlugResource payloads to bounded global
     * descriptor arrays, writes per-view DecalGPU/SlugLayerGPU records, and
     * dispatches the compute shader that builds each screen tile's decal list.
     */
    class ROCKY_EXPORT DecalSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, DecalSystemNode>
    {
    public:
        //! Construct the system
        DecalSystemNode(Registry& registry);

        //! Draw receiving-volume edges for all submitted decals, independently of cell-list limits.
        //! Change these settings during update; disabled debug drawing records no graphics commands.
        bool debugVolumes = false;
        bool debugVolumesSeeThrough = true;

        //! Renderer-owned graphics node; attach once to the main scene, not the compute graph.
        vsg::ref_ptr<vsg::Node> debugNode() const { return _debugNode; }

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    public: // vsg::Object
        void traverse(vsg::RecordTraversal&) const override;
        void traverse(vsg::ConstVisitor&) const override;
        void traverse(vsg::Visitor&) override;

    private:

        //! Per-view compute commands. The commands cull logical decals into
        //! that view's screen-tile lists before terrain fragments consume them.
        struct ViewDetail
        {
            vsg::ref_ptr<vsg::Commands> commands;
        };
        mutable ViewLocal<ViewDetail> _views;
        std::shared_ptr<SharedRenderData> _sharedRenderData;
        vsg::ref_ptr<vsg::Node> _debugNode;

        vsg::ref_ptr<vsg::ShaderStage> _cullingShader;

        // Descriptor slots whose owning style component was removed. They are
        // released during update(), where the shared descriptor arena is available.
        std::mutex _pendingTextureSlotsMutex;
        std::vector<std::int32_t> _pendingTextureSlots;
#ifdef ROCKY_HAS_SLUGHORN
        std::vector<std::int32_t> _pendingSlugSlots;
#endif

        // Kept outside the descriptor arena so slot zero remains usable.
        vsg::ref_ptr<vsg::ImageInfo> _fallbackTexture;
#ifdef ROCKY_HAS_SLUGHORN
        vsg::ref_ptr<vsg::ImageInfo> _fallbackSlugCurve;
        vsg::ref_ptr<vsg::ImageInfo> _fallbackSlugBand;
#endif

        // Prevent descriptor-capacity warnings from flooding the log every frame.
        bool _textureSlotsExhausted = false;
#ifdef ROCKY_HAS_SLUGHORN
        bool _slugSlotsExhausted = false;
#endif

        void rebuildCommands(ViewIDType, VSGContext);
        void updateStyles(VSGContext);
        void resizeGPUBuffersIfNeeded(VSGContext);
        void updateDecalsSSBO(VSGContext);

        void on_construct_Decal(entt::registry& r, entt::entity e);
        void on_construct_DecalStyle(entt::registry& r, entt::entity e);
        void on_construct_Overlay(entt::registry& r, entt::entity e);
        void on_construct_ProjectedTexture(entt::registry& r, entt::entity e);
        void on_construct_TextureResource(entt::registry& r, entt::entity e);
#ifdef ROCKY_HAS_SLUGHORN
        void on_construct_SlugResource(entt::registry& r, entt::entity e);
#endif
        void on_destroy_Decal(entt::registry& r, entt::entity e);
        void on_destroy_DecalStyle(entt::registry& r, entt::entity e);
        void on_destroy_Overlay(entt::registry& r, entt::entity e);
        void on_destroy_ProjectedTexture(entt::registry& r, entt::entity e);
        void on_destroy_TextureResource(entt::registry& r, entt::entity e);
#ifdef ROCKY_HAS_SLUGHORN
        void on_destroy_SlugResource(entt::registry& r, entt::entity e);
#endif
        void on_destroy_TextureSlotDetail(entt::registry& r, entt::entity e);
#ifdef ROCKY_HAS_SLUGHORN
        void on_destroy_SlugSlotDetail(entt::registry& r, entt::entity e);
#endif
        void on_update_Decal(entt::registry& r, entt::entity e);
        void on_update_DecalStyle(entt::registry& r, entt::entity e);
        void on_update_Overlay(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::DecalSystemNode)
