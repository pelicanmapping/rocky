/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Line.h>
#include <rocky_vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
     * ECS system that handles LineString components
     */
    class ROCKY_VSG_EXPORT LineSystem :  public vsg::Inherit<ECS::SystemNode, LineSystem>
    {
    public:
        //! Construct the system
        LineSystem(entt::registry&);

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
}
