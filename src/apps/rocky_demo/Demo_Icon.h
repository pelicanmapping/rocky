/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>


#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Icon = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    if (entity == entt::null)
    {
        // Load an icon image
        auto io = app.instance.io();
        auto image = io.services.readImageFromURI("https://user-images.githubusercontent.com/326618/236923465-c85eb0c2-4d31-41a7-8ef1-29d34696e3cb.png", io);
        if (image.status.failed())
        {
            status = image.status;
            return;
        }

        // Make an entity to host our icon:
        entity = app.entities.create();

        // Attach the new Icon and set up its properties:
        auto& icon = app.entities.emplace<Icon>(entity);
        icon.image = image.value;
        icon.style = IconStyle{ 75, 0.0f }; // pixel size, rotation(radians)

        // Transform to place the icon:
        auto& xform = app.entities.emplace<Transform>(entity);
        xform.setPosition(GeoPoint(SRS::WGS84, 0, 0, 50000));
    }

    if (ImGuiLTable::Begin("icon"))
    {
        auto& icon = app.entities.get<Icon>(entity);

        ImGuiLTable::Checkbox("Visible", &icon.active);

        if (ImGuiLTable::SliderFloat("Pixel size", &icon.style.size_pixels, 1.0f, 1024.0f))
            icon.dirty();

        if (ImGuiLTable::SliderFloat("Rotation", &icon.style.rotation_radians, 0.0f, 6.28f))
            icon.dirty();

        ImGuiLTable::End();
    }
};
