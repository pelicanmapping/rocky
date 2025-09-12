/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <rocky/Image.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    struct WidgetInstance;

    /**
    * Widget ECS component
    */
    struct Widget
    {
        //! Default text to display when "render" is not set
        std::string text;

        //! Custom render function
        std::function<void(WidgetInstance&)> render;
    };

    //! Structure that is passed to the Widget custom render function, giving
    //! you access to a variety of information and convenience functions.
    struct WidgetInstance
    {
        struct Widget& widget;
        const std::string& uid;
        entt::registry& registry;
        entt::entity entity;
        int windowFlags;
        ImVec2 center;
        ImVec2& size;
        ImGuiContext* context;
        std::uint32_t viewID;

        //! Submit a widget rendering function (convenience function).
        //! Don't forget to call ImGui::SetCurrentContext(i.context) or use
        //! ImGuiContextScope to set the current context before calling this.
        template<typename FUNC>
        inline void renderWindow(FUNC&& f) {
            ImGui::SetNextWindowPos(ImVec2(center.x - size.x / 2, center.y - size.y / 2));
            ImGui::Begin(uid.c_str(), nullptr, windowFlags);
            f();
            size = ImGui::GetWindowSize();
            ImGui::End();
        }
    };

    /**
    * RAII helper to set and restore the current ImGui context.
    */
    struct ImGuiContextScope
    {
        ImGuiContext* previous = nullptr;
        inline ImGuiContextScope(ImGuiContext* newContext)
        {
            previous = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(newContext);
        }
        inline ~ImGuiContextScope()
        {
            ImGui::SetCurrentContext(previous);
        }
    };
}

#endif
