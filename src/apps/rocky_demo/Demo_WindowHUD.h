/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "WindowImGuiOverlay.h"
#include <cstdio>

//! Draws passive corner annotations; bottomInset leaves room for the demo's map attribution.
inline void drawWindowHUD(ImGuiContext* context, const ImVec2& mouse, float bottomInset)
{
    ImGui::SetCurrentContext(context);
    const auto size = ImGui::GetIO().DisplaySize;
    auto* draw = ImGui::GetForegroundDrawList();
    auto* font = ImGui::GetFont();
    const float margin = 16.0f;
    const float watermarkSize = 72.0f;
    const auto watermarkExtent = font->CalcTextSizeA(watermarkSize, FLT_MAX, 0.0f, "Rocky");
    ImVec2 watermarkPosition(size.x - margin - watermarkExtent.x, margin);
    draw->AddText(font, watermarkSize, ImVec2(watermarkPosition.x + 1, watermarkPosition.y + 1),
        IM_COL32(0, 0, 0, 80), "Rocky");
    draw->AddText(font, watermarkSize, watermarkPosition, IM_COL32(255, 255, 255, 115), "Rocky");

    char text[80];
    if (mouse.x >= 0 && mouse.y >= 0 && mouse.x < size.x && mouse.y < size.y)
        std::snprintf(text, sizeof(text), "Mouse: %.0f, %.0f px", mouse.x, mouse.y);
    else
        std::snprintf(text, sizeof(text), "Mouse: --, -- px");

    const auto textSize = ImGui::CalcTextSize(text);
    ImVec2 position(size.x - margin - textSize.x, size.y - margin - bottomInset - textSize.y);
    draw->AddRectFilled(ImVec2(position.x - 8, position.y - 5),
        ImVec2(position.x + textSize.x + 8, position.y + textSize.y + 5), IM_COL32(0, 0, 0, 140), 4.0f);
    draw->AddText(position, IM_COL32(255, 255, 255, 220), text);
}

//! Controls the application-owned HUD without attaching it to a map view.
inline void Demo_WindowHUD(rocky::Application& app)
{
    auto* state = app.viewer->getObject<WindowImGuiOverlay::State>("demo.window-hud");
    if (!state)
        return;

    if (ImGui::Checkbox("Show window HUD", &state->visible))
        app.vsgcontext->requestFrame();

    ImGui::TextWrapped("A click-through watermark and mouse position above every view in the main window.");
}
