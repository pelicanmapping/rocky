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
#include <functional>
#include <cstdint>
#include <imgui.h>

namespace ROCKY_NAMESPACE
{
    class WidgetSystem
    {
    public:
        using Function = std::function<void(std::uint32_t, struct ImGuiContext*)>;

        inline void preRecord(Function&& f) {
            _preRecordTasks.emplace_back(std::move(f));
        }

    protected:
        std::vector<Function> _preRecordTasks;
    };

    /**
     * Creates commands for rendering ImGui-based overlays
     */
    class WidgetSystemNode : public vsg::Inherit<vsg::Node, WidgetSystemNode>, 
        public System,
        public WidgetSystem
    {
    public:
        //! Construct the mesh renderer
        WidgetSystemNode(Registry& registry);

        //! One time setup of the system
        void initialize(VSGContext& context) override;

        //! Per-frame update
        void update(VSGContext& context) override;
    };
}
#endif // defined(ROCKY_HAS_IMGUI)
