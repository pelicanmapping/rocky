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
    class ROCKY_VSG_EXPORT IconSystem : public vsg::Inherit<ECS::SystemNode, IconSystem>
    {
    public:
        //! Construct the mesh renderer
        IconSystem(entt::registry& registry);

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 1
        };

        static int featureMask(const Icon& icon);

        void initialize(Runtime&) override;

        ROCKY_VSG_SYSTEM_HELPER(Icon, helper);
    };
}
