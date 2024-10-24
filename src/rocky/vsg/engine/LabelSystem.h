/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>
#include <rocky/vsg/ECS.h>
#include <vsg/text/GpuLayoutTechnique.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT LabelSystemNode :
        public vsg::Inherit<ECS::SystemNode, LabelSystemNode>
    {
    public:
        //! Construct the mesh renderer
        LabelSystemNode(entt::registry& registry);

        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 0
        };

        //static int featureMask(const Label& component);

        //! One time setup of the system
        void initializeSystem(Runtime&) override;

        ROCKY_SYSTEMNODE_HELPER(Label, helper);

    private:
        void initializeComponent(Label& component, InitContext& context);

        vsg::ref_ptr<vsg::GpuLayoutTechnique> text_technique_shared;
    };
}
