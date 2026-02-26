/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/VSGContext.h>

#if defined(ROCKY_HAS_IMGUI)
#include <rocky/vsg/ecs/System.h>
#include <rocky/ecs/Registry.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering ImGui-based overlays
     */
    class WidgetSystemNode : public vsg::Inherit<vsg::Node, WidgetSystemNode>,
        public System
    {
    public:
        //! Construct the mesh renderer
        WidgetSystemNode(Registry& registry);

        //! One time setup of the system
        void initialize(VSGContext context) override;

        //! Per-frame update
        void update(VSGContext context) override;
    };
}
#endif // defined(ROCKY_HAS_IMGUI)
