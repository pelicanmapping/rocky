/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_GeoTransform = [](Application& app)
{
    static double scale = 500000.0;
    static double radius = 0.0;
    static vsg::ref_ptr<GeoTransform> geotransform;

    if (geotransform == nullptr)
    {
        // Create a simple VSG model using the Builder.
        vsg::Builder builder;
        vsg::GeometryInfo gi;
        gi.color = to_vsg(StockColor::Lime);
        auto node = builder.createCone(gi, vsg::StateInfo{});

        // Compile it.
        app.vsgcontext->compile(node);

        vsg::ComputeBounds cb;
        node->accept(cb);
        radius = vsg::length(cb.bounds.max - cb.bounds.min) * 0.5;

        // GeoTransform to position out node. No ECS required!
        geotransform = GeoTransform::create();

        geotransform->position = GeoPoint(SRS::WGS84, 3, 47.1, 500000.0);
        geotransform->topocentric = true;
        geotransform->localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
        geotransform->radius = radius * scale;

        geotransform->addChild(node);

        app.mapNode->addChild(geotransform);

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("geotransform"))
    {
        if (ImGuiLTable::SliderDouble("Latitude", &geotransform->position.y, -85.0, 85.0, "%.1lf"))
            geotransform->dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &geotransform->position.x, -180.0, 180.0, "%.1lf"))
            geotransform->dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &geotransform->position.z, 0.0, 2500000.0, "%.1lf"))
            geotransform->dirty();

        if (ImGuiLTable::Checkbox("Topocentric", &geotransform->topocentric))
            geotransform->dirty();

        if (ImGuiLTable::Checkbox("Frustum culling", &geotransform->frustumCulled))
            geotransform->dirty();

        if (ImGuiLTable::Checkbox("Horizon culling", &geotransform->horizonCulled))
            geotransform->dirty();

        auto rot = quaternion_from_matrix<glm::dquat>(geotransform->localMatrix);
        auto [pitch, roll, heading] = euler_degrees_from_quaternion(rot);

        if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            geotransform->localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            geotransform->dirty();
        }

        if (ImGuiLTable::SliderDouble("Pitch", &pitch, -180.0, 180.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            geotransform->localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            geotransform->dirty();
        }

        if (ImGuiLTable::SliderDouble("Roll", &roll, -180.0, 180.0, "%.1lf"))
        {
            auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
            geotransform->localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            geotransform->dirty();
        }

        if (ImGuiLTable::SliderDouble("Scale", &scale, 1.0, 2000000.0, "%.1lf", ImGuiSliderFlags_Logarithmic))
        {
            geotransform->localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            geotransform->dirty();
            geotransform->radius = radius * scale;
        }

        ImGuiLTable::End();
    }
};
