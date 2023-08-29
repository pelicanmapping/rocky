/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <vsg/io/read.h>
#include <vsg/nodes/MatrixTransform.h>

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
        ImGui::Text("Wait...");

        // Load model data from a URI
        URI uri("https://raw.githubusercontent.com/vsg-dev/vsgExamples/master/data/models/teapot.vsgt");
        auto result = uri.read(IOOptions());
        status = result.status;
        if (status.failed())
            return;

        // Parse the model
        // this is a bit awkward but it works when the URI has an extension
        auto options = vsg::Options::create(*app.instance.runtime().readerWriterOptions);
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

        // The model component
        auto& component = app.entities.emplace<ECS::NodeComponent>(entity);
        component.name = "Demo Model";
        component.node = scaler;

        // Since we're supplying our own node, we need to compile it manually
        app.instance.runtime().compile(component.node);

        // A transform component to place it
        auto& transform = app.entities.emplace<EntityTransform>(entity);
        transform.node->setPosition(GeoPoint(SRS::WGS84, 50, 0, 250000));
    }

    if (ImGuiLTable::Begin("model"))
    {
        auto& component = app.entities.get<ECS::NodeComponent>(entity);
        ImGuiLTable::Checkbox("Visible", &component.active);

        auto& transform = app.entities.get<EntityTransform>(entity);
        auto& xform = transform.node;

        if (ImGuiLTable::SliderDouble("Latitude", &xform->position.y, -85.0, 85.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &xform->position.x, -180.0, 180.0, "%.1lf"))
            xform->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &xform->position.z, 0.0, 2500000.0, "%.1lf"))
            xform->dirty();

        ImGuiLTable::End();
    }
};
