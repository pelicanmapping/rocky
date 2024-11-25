/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Label.h>
#include <rocky/vsg/ecs/ECSNode.h>
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
        LabelSystemNode(ecs::Registry& registry);

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //! One time setup of the system
        void initializeSystem(Runtime&) override;

        void createOrUpdateNode(const Label&, ecs::BuildInfo&, Runtime&) const;

    };
}
