/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Icon = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    // Custom component that holds icon properties.
    struct MyIcon
    {
        ImGuiImage iconImage;
        float sizePixels = 256.0f;
        float rotationDegrees = 0.0f;

        inline void render() {
            iconImage.render(ImVec2{ sizePixels, sizePixels }, rotationDegrees);
        }
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
        auto image = io.services().readImageFromURI("https://readymap.org/readymap/filemanager/download/public/icons/BENDER.png", io);
        if (image.failed())
        {
            status = image.error();
            return;
        }
        image.value()->flipVerticalInPlace();

        auto renderSimpleIcon = [&](WidgetInstance& i)
            {
                WidgetStyleEmpty withStyle(i);
                auto& icon = i.registry.get<MyIcon>(i.entity);
                icon.render();
            };

        // Make an entity to host our icon:
        entity = reg.create();

        // Attach the new Icon and set up its properties:
        auto& widget = reg.emplace<Widget>(entity);
        widget.render = renderSimpleIcon;

        auto& icon = reg.emplace<MyIcon>(entity);
        icon.iconImage = ImGuiImage(image.value(), app.vsgcontext);

        // Transform to place the icon:
        auto& transform = reg.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, 0, 0, 50000);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("icon"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& icon = reg.get<MyIcon>(entity);
        ImGuiLTable::SliderFloat("Pixel size", &icon.sizePixels, 1.0f, 1024.0f);
        ImGuiLTable::SliderFloat("Rotation", &icon.rotationDegrees, 0.0f, 360.0f);

        ImGuiLTable::End();
    }
};
