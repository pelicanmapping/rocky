/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <entt/entt.hpp>
#include <imgui.h>

namespace ROCKY_NAMESPACE
{
//#ifndef IMGUI_VERSION
//    // polyfill when imgui is not included (yet)
//    struct ImVec2 { float x; float y; };
//    using ImGuiContext = void;
//#endif

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
