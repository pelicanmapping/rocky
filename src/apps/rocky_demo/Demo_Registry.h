/** rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Registry.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Registry = [](Application& app)
{
    static std::set<entt::entity> entities;

    if (ImGuiLTable::Begin("registry_list"))
    {
        for(auto entity : entities)
        {
            ImGuiLTable::Text("Entity", "%d", (int)entity);

            ImGui::PushID((int)entity);
            if (ImGuiLTable::Button("Delete^"))
            {
                auto [lock, registry] = app.registry.write();
                registry.destroy(entity);
                entities.erase(entity);
            }
            ImGui::PopID();
        }

        ImGuiLTable::End();
    }

    ImGui::Separator();

    if (ImGuiLTable::Begin("registry_add"))
    {
        static double lat = 0.0, lon = 0.0, alt = 10000.0;
        ImGuiLTable::SliderDouble("Latitude", &lat, -85.0, 85.0, "%.1lf");
        ImGuiLTable::SliderDouble("Longitude", &lon, -180.0, 180.0, "%.1lf");
        ImGuiLTable::SliderDouble("Altitude", &alt, 0.0, 2500000.0, "%.1lf");
        
        if (ImGuiLTable::Button("Add"))
        {
            auto [lock, registry] = app.registry.write();

            auto entity = registry.create();
            entities.emplace(entity);

            auto& widget = registry.emplace<Widget>(entity);
            widget.text = "Entity_" + std::to_string((int)entity);

            auto& transform = registry.emplace<Transform>(entity);
            transform.position = GeoPoint(SRS::WGS84, lon, lat, alt);
        }

        if (ImGuiLTable::Button("Add and remove first"))
        {
            auto [lock, registry] = app.registry.write();

            auto entity = registry.create();
            entities.emplace(entity);

            auto& widget = registry.emplace<Widget>(entity);
            widget.text = "Entity_" + std::to_string((int)entity);

            auto& transform = registry.emplace<Transform>(entity);
            transform.position = GeoPoint(SRS::WGS84, lon, lat, alt);

            if (!entities.empty())
            {
                auto doomed = *entities.begin();
                entities.erase(doomed);
                registry.destroy(doomed);
            }
        }

        ImGuiLTable::End();
    }
};
