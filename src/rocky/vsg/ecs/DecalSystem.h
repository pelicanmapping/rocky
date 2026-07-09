/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Decal.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
    }

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
        // Called when a decal geometry component is found in the dirty list
        //void createOrUpdateGeometry(const DecalGeometry&, detail::DecalGeometryDetail&);

        // Called when a decal style is found in the dirty list
        //void createOrUpdateStyle(const DecalStyle&, detail::DecalStyleDetail&);

        // Called when a specific view's properties change (e.g. srs switch)
        //void createOrUpdateGeometryForView(ViewIDType, const DecalGeometry&, detail::DecalGeometryDetail&);



        // Non-view dependent state, initialized once
        vsg::ref_ptr<vsg::Dispatch> _dispatch;
        vsg::ref_ptr<vsg::BindComputePipeline> _bindPipeline;
        vsg::ref_ptr<vsg::DescriptorSetLayout> _descriptorSetLayout;
        mutable vsg::ref_ptr<vsg::Commands> _commands;

        // Per-view data, calculated during the record traversal
        struct ViewDetail
        {
            vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
            vsg::ref_ptr<vsg::Commands> commands;
            vsg::ref_ptr<vsg::DescriptorBuffer> lastFrustumParamsBuf;
            vsg::ref_ptr<vsg::DescriptorBuffer> lastFrustumsBuf;
        };
        mutable ViewLocal<ViewDetail> _views;

        std::shared_ptr<SharedRenderData> _sharedRenderData;

        // collection of all decals in the scene
        mutable unsigned _totalNumDecals = 0u;
        mutable vsg::ref_ptr<vsg::ubyteArray> _decalsData;
        mutable vsg::ref_ptr<vsg::DescriptorBuffer> _decalsBuf;

        // GPU-only collection of tiles passing GPU cull
        mutable vsg::ref_ptr<vsg::ubyteArray> _decalTilesData;
        mutable vsg::ref_ptr<vsg::DescriptorBuffer> _decalTilesBuf;

        void growGPUBuffersIfNeeded();

        void on_construct_Decal(entt::registry& r, entt::entity e);
        void on_construct_DecalStyle(entt::registry& r, entt::entity e);
        void on_destroy_Decal(entt::registry& r, entt::entity e);
        void on_destroy_DecalStyle(entt::registry& r, entt::entity e);
        void on_destroy_DecalStyleDetail(entt::registry& r, entt::entity e);
        void on_update_Decal(entt::registry& r, entt::entity e);
        void on_update_DecalStyle(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::DecalSystemNode)
