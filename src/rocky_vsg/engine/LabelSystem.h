/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Label.h>
#include <rocky_vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_VSG_EXPORT LabelSystem : public vsg::Inherit<ECS::SystemNode, LabelSystem>
    {
    public:
        //! Construct the mesh renderer
        LabelSystem(entt::registry& registry);

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
}
