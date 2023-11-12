/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT LabelSystemNode : public vsg::Inherit<ECS::VSG_SystemNode, LabelSystemNode>
    {
    public:
        //! Construct the mesh renderer
        LabelSystemNode(entt::registry& registry) :
            helper(registry) { }

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //static int featureMask(const Label& component);

        //! One time setup of the system
        void initialize(Runtime&) override;

        ROCKY_VSG_SYSTEM_HELPER(Label, helper);
    };

    class ROCKY_EXPORT LabelSystem : public ECS::VSG_System
    {
    public:
        LabelSystem(entt::registry& registry) :
            ECS::VSG_System(registry) { }

        vsg::ref_ptr<ECS::VSG_SystemNode> getOrCreateNode() override {
            if (!node)
                node = LabelSystemNode::create(registry);
            return node;
        }
    };
}
