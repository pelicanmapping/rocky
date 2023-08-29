/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Line.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Line_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        ImGui::Text("Wait...");

        entity = app.entities.create();
        auto& line = app.entities.emplace<Line>(entity);

        auto xform = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        const double alt = 125000;
        std::vector<glm::dvec3> points;
        for (double lon = -180.0; lon <= 0.0; lon += 2.5)
        {
            glm::dvec3 ecef;
            if (xform(glm::dvec3(lon, -20.0, alt), ecef))
                points.push_back(ecef);
        }
        line.push(points.begin(), points.end());
        line.style = LineStyle{ { 1,1,0,1 }, 3.0f, 0xffff, 4 };
        line.write_depth = true;
    }

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        auto& component = app.entities.get<Line>(entity);

        ImGuiLTable::Checkbox("Visible", &component.active);

        if (component.style.has_value())
        {
            float* col = (float*)&component.style->color;
            if (ImGuiLTable::ColorEdit3("Color", col))
                component.dirty();

            if (ImGuiLTable::SliderFloat("Width", &component.style->width, 1.0f, 15.0f, "%.0f"))
                component.dirty();

            if (ImGuiLTable::SliderInt("Stipple pattern", &component.style->stipple_pattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
                component.dirty();

            if (ImGuiLTable::SliderInt("Stipple factor", &component.style->stipple_factor, 1, 4))
                component.dirty();

            ImGuiLTable::End();
        }
    }
};

auto Demo_Line_Relative = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        ImGui::Text("Wait...");

        entity = app.entities.create();
        auto& line = app.entities.emplace<Line>(entity);

        const double size = 500000;
        std::vector<vsg::vec3> points = {
            {-size, -size, 0},
            { size, -size, 0},
            {    0,  size, 0},
            {-size, -size, 0} };
        line.push(points.begin(), points.end());
        line.style = LineStyle{ {1,0,0,1}, 4.0f };
        line.write_depth = true;

        // Position the transform
        auto& transform = app.entities.emplace<EntityTransform>(entity);
        transform.node->setPosition(GeoPoint(SRS::WGS84, -30.0, 10.0, 25000.0));
        transform.node->bound.radius = size; // for horizon culling
    }

    if (ImGuiLTable::Begin("relative linestring"))
    {
        auto& line = app.entities.get<Line>(entity);

        ImGuiLTable::Checkbox("Visible", &line.active);

        if (line.style.has_value())
        {
            if (ImGuiLTable::ColorEdit3("Color", (float*)&line.style->color))
                line.dirty();
        }

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
