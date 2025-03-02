/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Widget.h>
#include <rocky/vsg/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering ImGui-based overlays
     */
    class ROCKY_EXPORT WidgetSystemNode : public vsg::Inherit<vsg::Node, WidgetSystemNode>,
        public ecs::System
    {
    public:
        //! Construct the mesh renderer
        WidgetSystemNode(ecs::Registry& registry);

        //! One time setup of the system
        void initialize(VSGContext& context) override;

        //! Per-frame update
        void update(VSGContext& context) override;
    };
}
