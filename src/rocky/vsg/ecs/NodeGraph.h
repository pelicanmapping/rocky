/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/VSGContext.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /** ECS component that holds a VSG node */
    struct NodeGraph : public RevisionedComponent
    {
        vsg::ref_ptr<vsg::Node> node;
    };


    /**
    * VSG node that renders Node components (just plain vsg nodes)
    */
    class ROCKY_EXPORT NodeSystemNode : public vsg::Inherit<detail::SystemNode<NodeGraph>, NodeSystemNode>
    {
    public:
        NodeSystemNode(Registry& registry) :
            Inherit(registry)
        {
            //nop
        }

        void createOrUpdateNode(NodeGraph& component, detail::BuildInfo& data, VSGContext&) const override
        {
            data.new_node = component.node;
        }
    };
}
