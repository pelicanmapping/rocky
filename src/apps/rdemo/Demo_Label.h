/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Label.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Label = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    auto& font = app.instance.runtime().defaultFont;

    if (font.working())
    {
        ImGui::Text("Loading font, please wait...");
        return;
    }

    if (!font.available())
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
        label.font = font.value();
        
        // Attach a transform to place and move the label:
        auto& transform = app.entities.emplace<EntityTransform>(entity);
        transform.node->setPosition({ SRS::WGS84, -35.0, 15.0, 25000.0 });
    }

    if (ImGuiLTable::Begin("text"))
    {
        auto& label = app.entities.get<Label>(entity);
        ImGuiLTable::Checkbox("Visible", &label.active);

        char buf[256];
        strcpy(&buf[0], label.text.c_str());
        if (ImGuiLTable::InputText("Text", buf, 255))
        {
            label.text = std::string(buf);
            label.dirty();
        }

        auto& transform = app.entities.get<EntityTransform>(entity);
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
