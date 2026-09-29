/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Node that gets its content from a future (e.g., a node being loaded in
     * a background thread usually).
     */
    class /*ROCKY_EXPORT*/ FutureNode : public vsg::Inherit<vsg::Node, FutureNode>
    {
    public:
        using NodeResult = Result<vsg::ref_ptr<vsg::Node>>;

        //! Construct
        FutureNode(Future<NodeResult> future, VSGContext ctx);

    public:

        //! Disables the copy constructor.
        FutureNode(const FutureNode& rhs) = delete;

        void traverse(vsg::RecordTraversal&) const override;
        void traverse(vsg::Visitor& visitor) override;
        void traverse(vsg::ConstVisitor& visitor) const override;

        //! Resolve an available future without requiring a record traversal.
        //! Returns true once a child node is ready.
        bool resolve() const;
        bool ready() const { return resolve(); }

    private:
        mutable Future<NodeResult> _future;
        mutable vsg::ref_ptr<vsg::Node> _child;
        VSGContext _vsgcontext;
    };

} // namespace
