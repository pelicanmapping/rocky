/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <rocky/vsg/Common.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    struct WidgetInstance
    {
        struct Widget& widget;
        const std::string& uid;
        entt::registry& registry;
        entt::entity entity;
        const int defaultWindowFlags;
        ImVec2 position;
        ImVec2& size;
        ImGuiContext* context;
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

#endif
