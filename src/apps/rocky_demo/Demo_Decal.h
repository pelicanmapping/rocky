/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Decal = [](Application& app)
{
    static entt::entity entity = entt::null;
    static double scale = 10000.0;

    if (entity == entt::null)
    {
        auto [_, reg] = app.registry.write();

        entity = reg.create();

        auto& decal = reg.emplace<Decal>(entity);
        (void)decal;

        // A transform component to place and move it on the map
        auto& transform = reg.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, 0.0, 51.50, 0.0);
        transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
        transform.topocentric = true;

        app.vsgcontext->requestFrame();
    }

    auto [_, reg] = app.registry.read();

    auto& decal = reg.get<Decal>(entity);

    if (ImGuiLTable::Begin("decal"))
    {
        auto& v = reg.get<Visibility>(entity).visible[0];

        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& transform = reg.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty(reg);

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty(reg);

        //if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf", ImGuiSliderFlags_Logarithmic))
        //    transform.dirty(reg);

        auto rot = quaternion_from_matrix<glm::dquat>(transform.localMatrix);

        auto [pitch, roll, heading] = euler_degrees_from_quaternion(rot);

        if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            transform.dirty(reg);
        }

        if (ImGuiLTable::SliderDouble("Pitch", &pitch, -90.0, 90.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            transform.dirty(reg);
        }

        if (ImGuiLTable::SliderDouble("Roll", &roll, -90.0, 90.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            transform.dirty(reg);
        }

        if (ImGuiLTable::SliderDouble("Scale", &scale, 1.0, 100000.0, "%.1lf", ImGuiSliderFlags_Logarithmic))
        {
            transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            transform.dirty(reg);
        }

        ImGuiLTable::End();
    }
};
