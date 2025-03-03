/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <entt/entt.hpp>

#ifdef ROCKY_HAS_IMGUI
#include <imgui.h>
#endif

namespace ROCKY_NAMESPACE
{
#ifdef ROCKY_HAS_IMGUI
    using WidgetVec2 = ImVec2;
    using WidgetContext = ImGuiContext;
#else
    struct ImVec2 { float x; float y; };
    using ImGuiContext = void;
#endif

    struct WidgetInstance
    {
        struct Widget& widget;
        const std::string& uid;
        entt::registry& registry;
        entt::entity entity;
        const int defaultWindowFlags;
        WidgetVec2 position;
        WidgetVec2& size;
        WidgetContext* context;
        std::uint32_t viewID;
    };

    /**
    * Widget component
    */
    struct Widget
    {
        //! Default text to display when "render" is not set
        std::string text;
        
        //! Custom render function; x and y are screen coordinates
        std::function<void(WidgetInstance&)> render;
    };
}
