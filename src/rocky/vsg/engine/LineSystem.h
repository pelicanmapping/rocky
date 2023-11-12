/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
     * ECS system that handles LineString components
     */
    class ROCKY_EXPORT LineSystemNode :  public vsg::Inherit<ECS::VSG_SystemNode, LineSystemNode>
    {
    public:
        //! Construct the system
        LineSystemNode(entt::registry& registry) :
            helper(registry) { }

        enum Features
        {
            DEFAULT = 0x0,
            WRITE_DEPTH = 1 << 0,
            NUM_PIPELINES = 2
        };

        static int featureMask(const Line&);

        void initialize(Runtime&) override;

        ROCKY_VSG_SYSTEM_HELPER(Line, helper);
    };

    class ROCKY_EXPORT LineSystem : public ECS::VSG_System
    {
    public:
        LineSystem(entt::registry& registry) :
            ECS::VSG_System(registry) { }

        vsg::ref_ptr<ECS::VSG_SystemNode> getOrCreateNode() override {
            if (!node)
                node = LineSystemNode::create(registry);
            return node;
        }
    };
}
