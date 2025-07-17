/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/io/read.h>
#include <rocky/URI.h>
#include "helpers.h"
#include <filesystem>

using namespace ROCKY_NAMESPACE;

auto Demo_Model = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;
    const double scale = 50000.0;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model load failed!");
        return;
    }

    if (entity == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        // Load model data from a URI
        URI uri("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");
        auto result = uri.read(app.vsgcontext->io);
        status = result.status;
        if (status.failed())
            return;

        // Parse the model
        // this is a bit awkward but it works when the URI has an extension
        auto options = vsg::Options::create(*app.vsgcontext->readerWriterOptions);
        auto extension = std::filesystem::path(uri.full()).extension();
        options->extensionHint = extension.empty() ? std::filesystem::path(result.value.contentType) : extension;
        std::stringstream in(result.value.data);
        auto node = vsg::read_cast<vsg::Node>(in, options);
        if (!node)
        {
            status = Status(Status::ResourceUnavailable, "Failed to parse model");
            return;
        }

        // New entity to host our model
        entity = registry.create();

        // The model component; we just set the node directly.
        auto& model = registry.emplace<NodeGraph>(entity);
        model.node = node;

        // A transform component to place and move it on the map
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = GeoPoint(SRS::WGS84, 50, 0, 250000);
        transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto [lock, registry] = app.registry.read();

        bool v = visible(registry, entity);
        if (ImGuiLTable::Checkbox("Show", &v))
            setVisible(registry, entity, v);

        auto& transform = registry.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        auto rot = util::quaternion_from_matrix<glm::dquat>(transform.localMatrix);

        auto [pitch, roll, heading] = util::euler_degrees_from_quaternion(rot);

        if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
        {
            auto rot = util::quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale)) * glm::mat4_cast(rot);
            transform.dirty();
        }

        if (ImGuiLTable::SliderDouble("Pitch", &pitch, -90.0, 90.0, "%.1lf"))
        {
            auto rot = util::quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale)) * glm::mat4_cast(rot);
            transform.dirty();
        }

        if (ImGuiLTable::SliderDouble("Roll", &roll, -90.0, 90.0, "%.1lf"))
        {
            auto rot = util::quaternion_from_euler_degrees(pitch, roll, heading);
            transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale)) * glm::mat4_cast(rot);
            transform.dirty();
        }

        ImGuiLTable::End();
    }
};
