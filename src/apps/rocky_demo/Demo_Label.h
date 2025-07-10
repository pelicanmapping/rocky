/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Label = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    auto& font = app.vsgcontext->defaultFont;
    if (!font)
    {
        ImGui::TextWrapped(status.message.c_str());
        status = Status(Status::ResourceUnavailable,
            "No font available - did you set the ROCKY_DEFAULT_FONT environment variable?");
        return;
    }

    if (entity == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        // Create a host entity
        entity = registry.create();

        // Attach a label to the host and configure it
        auto& label = registry.emplace<Label>(entity);
        label.text = "Hello, world";
        label.style.font = font;
        label.style.pointSize = 36.0f;
        label.style.outlineSize = 0.05f;
        
        // Attach a transform to place and move the label:
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, -35.0, 15.0, 15000.0);
    }

    if (ImGuiLTable::Begin("text"))
    {
        auto [lock, registry] = app.registry.read();

        bool visible = ecs::visible(registry, entity);
        if (ImGuiLTable::Checkbox("Show", &visible))
            ecs::setVisible(registry, entity, visible);

        auto& label = registry.get<Label>(entity);

        if (label.text.length() <= 255)
        {
            char buf[256];
            std::copy(label.text.begin(), label.text.end(), buf);
            buf[label.text.length()] = '\0';
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                label.text = std::string(buf);
                label.dirty();
            }
        }

        if (ImGuiLTable::SliderFloat("Point size", &label.style.pointSize, 8.0f, 144.0f, "%.0f"))
            label.dirty();

        if (ImGuiLTable::SliderFloat("Outline size", &label.style.outlineSize, 0.0f, 0.5f, "%.2f"))
            label.dirty();

        auto& transform = registry.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};
