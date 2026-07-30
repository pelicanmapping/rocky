/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Optics.h>
#include <rocky/vsg/ecs/ECSNode.h>

namespace ROCKY_NAMESPACE
{
    /**
     * ECS system that handles Optics components
     */
    class ROCKY_EXPORT OpticsSystemNode : public vsg::Inherit<detail::SimpleSystemNodeBase, OpticsSystemNode>
    {
    public:
        //! Construct the system
        OpticsSystemNode(Registry& registry);

        //! Target geometry for decals
        vsg::observer_ptr<vsg::Node> target;

    public: // SimpleSystemNodeBase
        void initialize(VSGContext) override;
        void update(VSGContext) override;

    private:
        void updateOptics(VSGContext);
    };
}

EVSG_type_name(rocky::OpticsSystemNode)
