/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Model = [](Application& app)
{
    static entt::entity entity = entt::null;
    const double scale = 50000.0;

    if (entity == entt::null)
    {
        auto [_, reg] = app.registry.write();

        // Create a simple VSG model using the Builder.
        vsg::Builder builder;
        vsg::GeometryInfo gi;
        gi.color = to_vsg(Color::Cyan);
        auto node = builder.createSphere(gi, vsg::StateInfo{});
        app.vsgcontext->compile(node);

        // New entity to host our model
        entity = reg.create();

        // The model component; we just set the node directly.
        auto& model = reg.emplace<NodeGraph>(entity);
        model.node = node;

        // A transform component to place and move it on the map
        auto& transform = reg.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, 50, 0, 250000);
        transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
        transform.topocentric = true;

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto [_, reg] = app.registry.read();

        auto& v = reg.get<Visibility>(entity).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(reg, entity, v);

        auto& transform = reg.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty(reg);

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty(reg);

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty(reg);

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

        ImGuiLTable::End();
    }
};
