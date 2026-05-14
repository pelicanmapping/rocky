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
    static std::vector<std::string> fonts;

    std::string scriptCode = R"(
        function onStart()
            print("onStart called for entity " .. tostring(self.id))
        end

        function onDestroy()
            print("onDestroy called for entity " .. tostring(self.id))
        end

        num_updates = 0

        function onUpdate()
            num_updates = num_updates + 1

            local label = self:getComponent("Label")
            local style = self:getComponent("LabelStyle")
            local transform = self:getComponent("Transform")
            if label == nil or style == nil or transform == nil then
                return
            end

            if base_lon == nil then
                base_lon = transform.position.x
                base_lat = transform.position.y
                base_alt = transform.position.z
            end

            local t = num_updates * 0.025
            local lon = base_lon + math.sin(t) * 8.0
            local lat = base_lat + math.cos(t * 0.7) * 3.0
            local r = 0.5 + math.sin(t * 2.1) * 0.5
            local g = 0.5 + math.sin(t * 1.7 + 2.1) * 0.5
            local b = 0.5 + math.sin(t * 1.3 + 4.2) * 0.5

            transform.position.x = lon
            transform.position.y = lat
            transform.position.z = base_alt
            self:dirty("Transform")

            style.textColor.r = r
            style.textColor.g = g
            style.textColor.b = b
            style.textColor.a = 1.0
            self:dirty("LabelStyle")

            label.text = string.format("id=%s pos=%.2f, %.2f, %.2f", tostring(self.id), lon, lat, base_alt)
            self:dirty("Label")
        end
    )";

    if (entity == entt::null)
    {
        auto [lock, reg] = app.registry.write();

        {
            // Create a host entity
            entity = reg.create();

            // Style for our label
            auto& style = reg.emplace<LabelStyle>(entity);
            style.textSize = 18.0f;
            style.outlineSize = 1.0f;
            style.backgroundColor = Color(0, 0, 0.5f, 0.75f);
            style.borderSize = 1.0f;
            style.borderColor = StockColor::Cyan;
            style.padding = { 4.0f, 3.0f };
            style.fontName = std::filesystem::path(ROCKY_DEMO_DEFAULT_FONT).lexically_normal().string();

            // Label data to render in the widget
            auto& label = reg.emplace<Label>(entity, "Entity 1", style);

            // Attach a transform to place and move the label:
            auto& transform = reg.emplace<Transform>(entity);
            transform.position = GeoPoint(SRS::WGS84, -35.0, 20.0, 15000.0);

            // Add a script to the label.
            reg.emplace<Script>(entity, scriptCode);
        }


        {
            // Create a host entity
            entity = reg.create();

            // Style for our label
            auto& style = reg.emplace<LabelStyle>(entity);
            style.textSize = 18.0f;
            style.outlineSize = 1.0f;
            style.backgroundColor = Color(1, 0, 0.5f, 0.75f);
            style.borderSize = 1.0f;
            style.borderColor = StockColor::Cyan;
            style.padding = { 4.0f, 3.0f };
            style.fontName = std::filesystem::path(ROCKY_DEMO_DEFAULT_FONT).lexically_normal().string();

            // Label data to render in the widget
            auto& label = reg.emplace<Label>(entity, "Entity 2", style);

            // Attach a transform to place and move the label:
            auto& transform = reg.emplace<Transform>(entity);
            transform.position = GeoPoint(SRS::WGS84, -50.0, 20.0, 15000.0);

            // Add a script to the label.
            reg.emplace<Script>(entity, scriptCode);
        }

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
            if (ImGuiLTable::InputText("Text", &buf[0], 255))
            {
                label.text = std::string(buf);
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

        if (ImGuiLTable::SliderFloat("Outline size", &style.outlineSize, 0.0f, 3.0f, "%.0f"))
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

        if (ImGuiLTable::SliderFloat("Pivot X", &style.pivot.x, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderFloat("Pivot Y", &style.pivot.y, 0.0f, 1.0f, "%.2f"))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderInt("Offset X", &style.offset.x, -500, 500))
            app.vsgcontext->requestFrame();

        if (ImGuiLTable::SliderInt("Offset Y", &style.offset.y, -500, 500))
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
