/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>
#include <rocky/URI.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Model = [](Application& app)
{
    static entt::entity entity = entt::null;
    static Status status;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Model load failed!");
        return;
    }

    if (entity == entt::null)
    {
        // Load model data from a URI
        URI uri("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");
        auto result = uri.read(app.instance.io());
        status = result.status;
        if (status.failed())
            return;

        // Parse the model
        // this is a bit awkward but it works when the URI has an extension
        auto options = vsg::Options::create(*app.runtime().readerWriterOptions);
        auto extension = std::filesystem::path(uri.full()).extension();
        options->extensionHint = extension.empty() ? std::filesystem::path(result.value.contentType) : extension;
        std::stringstream in(result.value.data);
        auto model = vsg::read_cast<vsg::Node>(in, options);
        if (!model)
        {
            status = Status(Status::ResourceUnavailable, "Failed to parse model");
            return;
        }

        // make it bigger so we can see it
        auto scaler = vsg::MatrixTransform::create();
        scaler->matrix = vsg::scale(250000.0);
        scaler->addChild(model);

        // New entity to host our model
        entity = app.entities.create();

        // The model component; we just set the node directly.
        auto& component = app.entities.emplace<ECS::NodeComponent>(entity);
        component.name = "Demo Model";
        component.node = scaler;

        // Since we're supplying our own node, we need to compile it manually
        app.runtime().compile(component.node);

        // A transform component to place and move it on the map
        auto& transform = app.entities.emplace<Transform>(entity);
        transform.setPosition(GeoPoint(SRS::WGS84, 50, 0, 250000));
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto& component = app.entities.get<ECS::NodeComponent>(entity);
        ImGuiLTable::Checkbox("Visible", &component.active);

        auto& transform = app.entities.get<Transform>(entity);
        auto& geo = transform.node;

        if (ImGuiLTable::SliderDouble("Latitude", &geo->position.y, -85.0, 85.0, "%.1lf"))
            geo->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &geo->position.x, -180.0, 180.0, "%.1lf"))
            geo->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &geo->position.z, 0.0, 2500000.0, "%.1lf"))
            geo->dirty();

        double heading, pitch, roll;
        vsg::dquat rot;
        get_rotation_from_matrix(transform.local_matrix, rot);
        rocky::quaternion_to_euler_degrees(rot, pitch, roll, heading);

        if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
        {
            rocky::euler_degrees_to_quaternion(pitch, roll, heading, rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        if (ImGuiLTable::SliderDouble("Pitch", &pitch, -90.0, 90.0, "%.1lf"))
        {
            rocky::euler_degrees_to_quaternion(pitch, roll, heading, rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        ImGuiLTable::End();
    }
};
