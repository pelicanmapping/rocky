/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

/** Demonstrates how to create your own Component type (MyIcon) and render it as a Widget. */

auto Demo_Icon = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    // Custom component that holds icon properties.
    struct MyIcon
    {
        ImGuiImage iconImage;
        float sizePixels = 64.0f;
        float rotationDegrees = 0.0f;
        glm::fvec2 pivot = glm::fvec2{ 0.5f, 0.5f };
    };

    static auto renderIcon = [](WidgetInstance& i)
        {
            auto& icon = i.registry.get<MyIcon>(i.entity);

            ImVec2 pivot{ icon.pivot.x - 0.5f, icon.pivot.y - 0.5f };
            float cos_a = cos(glm::radians(icon.rotationDegrees));
            float sin_a = sin(glm::radians(icon.rotationDegrees));
            pivot = ImRotate(pivot, cos_a, sin_a);
            pivot.x += 0.5f, pivot.y += 0.5f;

            ImGui::SetCurrentContext(i.context);
            ImGui::SetNextWindowBgAlpha(0.0f); // fully transparent background
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, (float)0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
            ImGui::SetNextWindowPos(ImVec2{ i.position.x, i.position.y }, ImGuiCond_Always, pivot);
            ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);

            icon.iconImage.render(ImVec2{ icon.sizePixels, icon.sizePixels }, icon.rotationDegrees);

            ImGui::End();
            ImGui::PopStyleVar(2);
        };

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", "Image load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", status.error().message.c_str());
        return;
    }

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        // Load an icon image
        auto io = app.vsgcontext->io;
        auto image = io.services().readImageFromURI("https://readymap.org/readymap/filemanager/download/public/icons/placemark32.png", io);
        if (image.failed())
        {
            status = image.error();
            return;
        }
        image.value()->flipVerticalInPlace();

        // Make an entity to host our icon:
        entity = reg.create();

        // Attach the new Icon and set up its properties:
        auto& widget = reg.emplace<Widget>(entity);
        widget.render = renderIcon;

        auto& icon = reg.emplace<MyIcon>(entity);
        icon.iconImage = ImGuiImage(image.value(), app.vsgcontext);
        icon.sizePixels = image.value()->width();
        icon.pivot = glm::fvec2{ 0.5f, 1.0f }; // bottom-center

        // Transform to place the icon:
        auto& transform = reg.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, 2.35, 48.8575, 0); // Paris, France

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("icon"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& icon = reg.get<MyIcon>(entity);
        ImGuiLTable::SliderFloat("Pixel size", &icon.sizePixels, 1.0f, 128.f, "%.0f");
        ImGuiLTable::SliderFloat("Rotation", &icon.rotationDegrees, 0.0f, 360.0f);

        ImGuiLTable::End();
    }
};
