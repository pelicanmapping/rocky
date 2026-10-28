/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
using namespace ROCKY_NAMESPACE;

#define LABEL_ICON_URI "https://readymap.org/readymap/filemanager/download/public/icons/placemark32.png"

auto Demo_Label = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;
    static std::vector<std::string> fonts;
    static Image::Ptr iconImage;

    if (entity == entt::null)
    {
        // Load an icon image
        auto io = app.vsgcontext->io;
        auto image = io.services().readImageFromURI(LABEL_ICON_URI, io);
        if (image.failed())
            Log()->warn("Unable to load icon for demo");
        if (image)
            image.value()->flipVerticalInPlace();
        iconImage = image ? image.value() : nullptr;

        auto [lock, reg] = app.registry.write();

        // Create a host entity
        entity = reg.create();

        // Style for our label
        auto& style = reg.emplace<LabelStyle>(entity);
        style.textSize = 18.0f;
        style.textOutlineSize = 1.0f;
        style.textPivot = { 0.0f, 0.5f };
        style.textOffset = { 16.0f, -18.0f };
        style.borderColor = StockColor::Cyan;
        style.padding = { 4.0f, 3.0f };
        style.fontName = std::filesystem::path(ROCKY_DEMO_DEFAULT_FONT).lexically_normal().string();
        style.icon = iconImage;
        style.iconPivot = { 0.5f, 1.0f };
        if (style.icon)
            style.iconSizePixels = (float)style.icon->height();

        // Label data to render in the widget
        auto& label = reg.emplace<Label>(entity, "London\nEngland\nUK", style);
        
        // Attach a transform to place and move the label:
        auto& transform = reg.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, -0.1278, 51.5074, 20.0); // London UK

        app.vsgcontext->requestFrame();

        // find fonts
#ifdef _WIN32
        std::filesystem::path fontFolder("C:/Windows/Fonts");
#else
        std::filesystem::path fontFolder("/usr/share/fonts");
#endif
        for (auto entry : std::filesystem::recursive_directory_iterator(fontFolder))
        {
            if (entry.path().extension() == ".ttf")
                fonts.push_back(entry.path().lexically_normal().string());
        }
    }

    if (ImGuiLTable::Begin("label:demo1"))
    {
        auto [lock, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& label = reg.get<Label>(entity);

        if (label.text.length() <= 255)
        {
            char buf[256];
            std::copy(label.text.begin(), label.text.end(), buf);
            buf[label.text.length()] = '\0';
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Text");
            ImGui::TableNextColumn();
            if (ImGui::InputTextMultiline("##LabelText", buf, sizeof(buf),
                ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f)))
            {
                label.text = std::string(buf);
                app.vsgcontext->requestFrame();
            }
        }

        auto& style = reg.get<LabelStyle>(label.style);

        if (!fonts.empty() && ImGuiLTable::StringCombo("Font", style.fontName, fonts))
        {
            style.dirty(reg);
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::SliderFloat("Text size", &style.textSize, 8.0f, 144.0f, "%.0f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Text outline size", &style.textOutlineSize, 0.0f, 3.0f, "%.0f"))
            app.vsgcontext->requestFrame();

        const char* justifications[] = { "Left", "Center", "Right" };
        int justify = static_cast<int>(style.textJustify);
        if (ImGuiLTable::Combo("Multiline justify", &justify, justifications, 3))
        {
            style.textJustify = static_cast<LabelStyle::Justify>(justify);
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::SliderFloat("Text pivot X", &style.textPivot.x, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Text pivot Y", &style.textPivot.y, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderInt("Text offset X", &style.textOffset.x, -500, 500))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderInt("Text offset Y", &style.textOffset.y, -500, 500))
            app.vsgcontext->requestFrame();

        bool iconVisible = style.icon != nullptr;
        if (ImGuiLTable::Checkbox("Icon", &iconVisible))
        {
            style.icon = iconVisible ? iconImage : nullptr;
            style.dirty(reg);
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::SliderFloat("Icon pivot X", &style.iconPivot.x, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Icon pivot Y", &style.iconPivot.y, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Icon rotation", &style.iconRotationDegrees, -180.0f, 180.0f, "%.0f deg"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Border width", &style.borderSize, 0.0f, 3.0f, "%.0f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::ColorEdit3("Border color", &style.borderColor[0]))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::ColorEdit4("Background color", &style.backgroundColor[0]))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Padding X", &style.padding.x, 0.0f, 10.0f, "%.0f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Padding Y", &style.padding.y, 0.0f, 10.0f, "%.0f"))
            app.vsgcontext->requestFrame();

        ImGuiLTable::End();


        ImGui::Separator();

        ImGuiLTable::Begin("label:demo2");

        auto& transform = reg.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty(reg);

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty(reg);

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty(reg);

        ImGuiLTable::End();
    }
};
