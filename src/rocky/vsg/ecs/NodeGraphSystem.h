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

        void createOrUpdateNode(const NodeGraph& component, detail::BuildInfo& data, VSGContext&) const override
        {
            data.new_node = component.node;
        }
    };
}
