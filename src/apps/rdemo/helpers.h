/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Application.h>
#include <vsgImGui/RenderImGui.h>
#include <vsg/commands/Command.h>

// handy nice-looking table with names on the left.
namespace ImGuiLTable
{
    static bool Begin(const char* name)
    {
        bool ok = ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingFixedFit);
        if (ok) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
        }
        return ok;
    }

    static void PlotLines(const char* label, float(*getter)(void*, int), void* data, int values_count, int values_offset, const char* overlay =nullptr,
        float scale_min = FLT_MAX, float scale_max = FLT_MAX)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::PlotLines("", getter, data, values_count, values_offset, overlay, scale_min, scale_max);
    }

    static bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = nullptr)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderFloat(s.c_str(), v, v_min, v_max, format);
    }

    static bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderFloat(s.c_str(), v, v_min, v_max, format, flags);
    }

    static bool SliderInt(const char* label, int* v, int v_min, int v_max)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderInt(s.c_str(), v, v_min, v_max);
    }

    static bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::SliderInt(s.c_str(), v, v_min, v_max, format, flags);
    }

    static bool Checkbox(const char* label, bool* v)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::Checkbox(s.c_str(), v);
    }

    static bool BeginCombo(const char* label, const char* defaultItem)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::BeginCombo(s.c_str(), defaultItem);
    }

    static void EndCombo()
    {
        return ImGui::EndCombo();
    }

    static bool InputFloat(const char* label, float* v)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::InputFloat(s.c_str(), v);
    }

    static bool InputText(const char* label, char* buf, std::size_t bufSize, ImGuiInputTextFlags flags = 0)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        return ImGui::InputText(label, buf, bufSize, flags);
    }

    static bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        return ImGui::ColorEdit3(label, col, flags);
    }

    static bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        return ImGui::ColorEdit4(label, col, flags);
    }

    template<typename...Args>
    static void Text(const char* label, const char* format, Args...args)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::Text(format, args...);
    }

    template<typename...Args>
    static void TextWrapped(const char* label, const char* format, Args...args)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::TextWrapped(format, args...);
    }

    static void Section(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), label);
        ImGui::TableNextColumn();
    }

    static void End()
    {
        ImGui::EndTable();
    }
}
