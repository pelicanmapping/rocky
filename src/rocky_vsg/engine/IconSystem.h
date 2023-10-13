/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Icon.h>
#include <rocky_vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    struct IconStyle;
    class Runtime;

    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_VSG_EXPORT IconSystemNode : public vsg::Inherit<ECS::VSG_SystemNode, IconSystemNode>
    {
    public:
        //! Construct the mesh renderer
        IconSystemNode(entt::registry& registry) :
            helper(registry) { }

        //! Features supported by this renderer
        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 1
        };

        //! Get the feature mask for a given icon
        static int featureMask(const Icon& icon);

        //! Initialize the system (once)
        void initialize(Runtime&) override;

        ROCKY_VSG_SYSTEM_HELPER(Icon, helper);
    };

    /**
    * ECS system for managing Icon components.
    * @see Icon
    */
    class ROCKY_VSG_EXPORT IconSystem : public ECS::VSG_System
    {
    public:
        IconSystem(entt::registry& registry) :
            ECS::VSG_System(registry) { }

        vsg::ref_ptr<ECS::VSG_SystemNode> getOrCreateNode() override {
            if (!node)
                node = IconSystemNode::create(registry);
            return node;
        }
    };
}
