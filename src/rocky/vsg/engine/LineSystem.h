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
    class LineSystem;

    /**
     * ECS system that handles LineString components
     */
    class ROCKY_EXPORT LineSystemNode :
        public vsg::Inherit<ECS::SystemNode, LineSystemNode>
    {
    public:
        //! Construct the system
        LineSystemNode(entt::registry& registry);

        enum Features
        {
            DEFAULT = 0x0,
            WRITE_DEPTH = 1 << 0,
            NUM_PIPELINES = 2
        };

        static int featureMask(const Line&);

        void initializeSystem(Runtime&) override;

        ROCKY_SYSTEMNODE_HELPER(Line, helper);

    private:
        void initializeComponent(Line& line, InitContext& context);
    };
}
