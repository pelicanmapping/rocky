/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Widget = [](Application& app)
{
#ifdef ROCKY_HAS_IMGUI
        
    static entt::entity entity = entt::null;
    static Status status;

    if (entity == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        entity = registry.create();

        auto& widget = registry.emplace<Widget>(entity);
        widget.text = "I'm a widget.";

        widget.render = [&](WidgetInstance& i)
            {
                static float some_float = 0;
                static int some_int = 0;
                static bool fixed_window_open = false;

                ImGui::SetCurrentContext(i.context);

                int flags = i.defaultWindowFlags;
                flags &= ~ImGuiWindowFlags_NoInputs;
                flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImVec2 pos{ i.position.x - i.size.x / 2, i.position.y - i.size.y / 2 };
                ImGui::SetNextWindowPos(pos);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);

                if (ImGui::Begin(i.uid.c_str(), nullptr, flags))
                {
                    ImGui::Text(i.widget.text.c_str());
                    ImGui::Separator();
                    ImGui::Text("Text string");
                    ImGui::SliderFloat("Slider", &some_float, 0.0f, 1.0f);
                    ImGui::Checkbox("Show me a fixed-position window", &fixed_window_open);

                    i.size = ImGui::GetWindowSize();
                }
                ImGui::End();
                ImGui::PopStyleVar();

                if (fixed_window_open)
                {
                    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x - 400, ImGui::GetMainViewport()->Size.y - 200));

                    if (ImGui::Begin("Fixed-position window"))
                    {
                        Transform& transform = i.registry.get<Transform>(i.entity);
                        auto p = transform.position.transform(SRS::WGS84);

                        ImGui::Text("Widgets can be placed at absolute coordinates too.");
                        ImGui::Separator();
                        if (ImGuiLTable::Begin("pos_table")) {
                            ImGuiLTable::Text("Latitude:", "%.3f", p.y);
                            ImGuiLTable::Text("Longitude:", "%.3f", p.x);
                            ImGuiLTable::Text("Altitude:", "%.1f", p.z);
                            ImGuiLTable::End();
                        }
                        ImGui::Separator();
                        if (ImGui::Button("Whatever"))
                            fixed_window_open = false;
                    }
                    ImGui::End();
                }
            };

        // Attach a transform to place and move the label:
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, -25.0, 25.0, 500000.0);

        // Drop line from the widget to the ground, for fun.
        auto& dropline = registry.emplace<Line>(entity);
        dropline.points = { { 0,0,0 }, { 0, 0, -500000 } };
        dropline.style.color = vsg::vec4{ 0, 1, 0, 1 };
        dropline.style.width = 2;
    }

    if (ImGuiLTable::Begin("widget_demo"))
    {
        auto [lock, registry] = app.registry.read();

        bool visible = ecs::visible(registry, entity);
        if (ImGuiLTable::Checkbox("Show", &visible))
        {
            ecs::setVisible(registry, entity, visible);
        }

        auto& widget = registry.get<Widget>(entity);

        if (widget.text.length() <= 255)
        {
            char buf[256];
            std::copy(widget.text.begin(), widget.text.end(), buf);
            buf[widget.text.length()] = '\0';
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                widget.text = std::string(buf);
            }
        }

        auto& transform = registry.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }

#endif
};
