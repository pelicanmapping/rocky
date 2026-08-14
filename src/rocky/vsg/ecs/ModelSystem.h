/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/ECSNode.h>
#include <queue>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        struct ModelDetail
        {
            vsg::ref_ptr<vsg::Node> node;
        };
    }

    /**
    * VSG node that loads and renders Model components
    */
    class ROCKY_EXPORT ModelSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, ModelSystemNode>
    {
    public:
        vsg::Node* renderTextureParticipant() override { return this; }
        void expandRenderTextureBounds(entt::registry&, entt::entity, RenderTextureBounds&, const SRS&) override;
        void contributeRenderTextureRevision(entt::registry&, entt::entity, RenderTextureRevision&) override;
        ModelSystemNode(Registry& registry);

        ~ModelSystemNode();

        //! One-time initialization of the system        
        void initialize(VSGContext) override;

        //! Every frame update
        void update(VSGContext) override;

        //! VSG record traversal
        void traverse(vsg::RecordTraversal&) const override;

        //! VSG visitor traversals
        void traverse(vsg::ConstVisitor& v) const override;
        void traverse(vsg::Visitor& v) override;

        // vsg::Compilable
        void compile(vsg::Context& cc) override;

    private:
        // render leaf for collecting and drawing meshes
        struct Drawable
        {
            vsg::Node* node = nullptr;
            TransformDetail* xformDetail = nullptr;
            Drawable(vsg::Node* node_, TransformDetail* xformDetail_) : node(node_), xformDetail(xformDetail_) {}
        };

        using DrawList = std::vector<Drawable>;
        mutable DrawList _drawList;

        using Loader = Future<Result<vsg::ref_ptr<vsg::Node>>>;
        struct LoadRecord {
            Loader promise;
            entt::entity entity;
        };
        std::queue<LoadRecord> _loaders;

        void on_construct_Model(entt::registry& r, entt::entity e);
        void on_destroy_Model(entt::registry& r, entt::entity e);
        void on_update_Model(entt::registry& r, entt::entity e);
    };
}

EVSG_type_name(rocky::ModelSystemNode)
