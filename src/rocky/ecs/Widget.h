/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#if defined(ROCKY_HAS_IMGUI) && __has_include(<imgui.h>)
#include <imgui.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    struct WidgetInstance;

    /**
    * Widget ECS component
    */
    struct Widget
    {
        //! Render function
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
        ImVec2 position;
        ImGuiContext* context;
        std::uint32_t viewID;
    };

    //! Helper RAII class for creating an non-styled widget window
    struct WidgetStyleEmpty
    {
        WidgetInstance& _i;

        WidgetStyleEmpty(WidgetInstance& i) : _i(i)
        {
            ImGui::SetCurrentContext(i.context);
            ImGui::SetNextWindowBgAlpha(0.0f); // fully transparent background
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, (float)0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
            ImGui::SetNextWindowPos(ImVec2{ i.position.x, i.position.y }, ImGuiCond_Always, ImVec2{ 0.5f, 0.5f });
            ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);
        }

        ~WidgetStyleEmpty()
        {
            //_i.size = ImGui::GetWindowSize();
            ImGui::End();
            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(2);
        }
    };
}

#endif
