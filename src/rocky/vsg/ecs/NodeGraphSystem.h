/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/ecs/ECSTypes.h>

namespace ROCKY_NAMESPACE
{
    struct TransformDetail;

    /**
    * VSG node that renders Node components (just plain vsg nodes)
    */
    class ROCKY_EXPORT NodeSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, NodeSystemNode>
    {
    public:
        NodeSystemNode(Registry& registry);

        ~NodeSystemNode();

        //! One-time initialization of the system        
        void initialize(VSGContext&) override;

        //! Every frame update
        void update(VSGContext&) override;

        //! VSG record traversal
        void traverse(vsg::RecordTraversal&) const override;

        //! VSG visitor traversal
        void traverse(vsg::ConstVisitor& v) const override;

        // vsg::Compilable
        void compile(vsg::Context& cc) override;

    private:
        mutable vsg::ref_ptr<vsg::MatrixTransform> _tempMT;

        // render leaf for collecting and drawing meshes
        struct Drawable
        {
            vsg::Node* node = nullptr;
            TransformDetail* xformDetail = nullptr;
            Drawable(vsg::Node* node_, TransformDetail* xformDetail) : node(node_), xformDetail(xformDetail) {}
        };

        using DrawList = std::vector<Drawable>;
        mutable DrawList _drawList;

        void on_construct_NodeGraph(entt::registry& r, entt::entity e);
        void on_update_NodeGraph(entt::registry& r, entt::entity e);
    };
}
