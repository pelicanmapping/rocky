/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Application.h>
#include <rocky/weejobs.h>
#include <vsg/commands/Command.h>
#include <chrono>

using namespace std::chrono_literals;

// utility to run a loop at a specific frequency (in Hz)
struct run_at_frequency
{
    run_at_frequency(float hertz) : start(std::chrono::steady_clock::now()), _max(1.0f / hertz) { }
    ~run_at_frequency() {
        const auto min_duration = std::chrono::microseconds(100); // prevent starvation.
        auto duration = _max - (std::chrono::steady_clock::now() - start);
        if (duration < min_duration) duration = min_duration;
        std::this_thread::sleep_for(duration);
        std::this_thread::yield();
    }
    auto elapsed() const { return std::chrono::steady_clock::now() - start; }
    std::chrono::steady_clock::time_point start;
    std::chrono::duration<float> _max;
};

const ImVec4 ImGuiErrorColor = ImVec4(1, 0.35f, 0.35f, 1);

#if(IMGUI_VERSION_NUM < 18928)
namespace ImGui
{
    static bool SeparatorText(const char* text)
    {
        ImGui::Separator();
        ImGui::Text(text);
        return true;
    }
}
#endif

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
        std::string s("##" + std::string(label));
        ImGui::PlotLines(s.c_str(), getter, data, values_count, values_offset, overlay, scale_min, scale_max);
    }

    static bool DragFloat(const char* label, float* v, float step, float v_min, float v_max, const char* format = nullptr)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::DragFloat(s.c_str(), v, step, v_min, v_max, format);
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

    static bool SliderDouble(const char* label, double* v, double v_min, double v_max, const char* format = nullptr)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        float temp = (float)*v;
        bool ok = ImGui::SliderFloat(s.c_str(), &temp, (float)v_min, (float)v_max, format);
        if (ok) *v = (double)temp;
        return ok;
    }

    static bool SliderDouble(const char* label, double* v, double v_min, double v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        float temp = (float)*v;
        bool ok = ImGui::SliderFloat(s.c_str(), &temp, (float)v_min, (float)v_max, format, flags);
        if (ok) *v = (double)temp;
        return ok;
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

    static bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        std::string s("##" + std::string(label));
        return ImGui::Combo(s.c_str(), current_item, items, items_count, height_in_items);
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

    //template<typename...Args>
    static void TextLinkOpenURL(const char* label, const char* text, const char* href)
    {
        ImGui::TableNextColumn();
        ImGui::Text(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::Text(text);
        // coming soon in imgui 1.91.0!
        //ImGui::TextLinkOpenURL(label, href);
    }

    static void Section(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), label);
        ImGui::TableNextColumn();
    }

    static bool Button(const char* label)
    {
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        return ImGui::Button(label);
    }

    static void End()
    {
        ImGui::EndTable();
    }
}
