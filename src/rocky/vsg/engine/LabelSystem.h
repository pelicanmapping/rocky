/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>
#include <rocky/vsg/ECS.h>
#include <vsg/text/Text.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT LabelSystemNode : public vsg::Inherit<ecs::SystemNode<Label>, LabelSystemNode>
    {
    public:
        //! Construct the mesh renderer
        LabelSystemNode(entt::registry& registry);

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //! One time setup of the system
        void initializeSystem(Runtime&) override;

        void createOrUpdateNode(entt::entity entity, CreateOrUpdateData& data, Runtime& runtime) const override;

    };
}
