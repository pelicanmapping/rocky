/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Label.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Label = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    auto& font = app.runtime().defaultFont;
    if (!font)
    {
        ImGui::TextWrapped(status.message.c_str());
        status = Status(Status::ResourceUnavailable,
            "No font available - did you set the ROCKY_DEFAULT_FONT environment variable?");
        return;
    }

    if (entity == entt::null)
    {
        // Create a host entity
        entity = app.entities.create();

        // Attach a label to the host and configure it
        auto& label = app.entities.emplace<Label>(entity);
        label.text = "Hello, world";
        label.style.font = font;
        label.style.pointSize = 36.0f;
        label.style.outlineSize = 0.05f;
        
        // Attach a transform to place and move the label:
        auto& transform = app.entities.emplace<Transform>(entity);
        transform.setPosition({ SRS::WGS84, -35.0, 15.0, 15000.0 });
    }

    if (ImGuiLTable::Begin("text"))
    {
        auto& label = app.entities.get<Label>(entity);
        ImGuiLTable::Checkbox("Visible", &label.visible);

        if (label.text.length() <= 255)
        {
            char buf[256];
            std::copy(label.text.begin(), label.text.end(), buf);
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                label.text = std::string(buf);
                label.dirty();
            }
        }

        if (ImGuiLTable::SliderFloat("Point size", &label.style.pointSize, 8.0f, 144.0f, "%.1f"))
            label.dirty();

        if (ImGuiLTable::SliderFloat("Outline size", &label.style.outlineSize, 0.0f, 0.5f, "%.2f"))
            label.dirty();

        auto& transform = app.entities.get<Transform>(entity);
        auto& xform = transform.node;

        if (ImGuiLTable::SliderDouble("Latitude", &xform->position.y, -85.0, 85.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &xform->position.x, -180.0, 180.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &xform->position.z, 0.0, 2500000.0, "%.1lf"))
            xform->dirty();

        ImGuiLTable::End();
    }
};
