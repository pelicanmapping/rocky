/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>
#include <rocky/vsg/Transform.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Line_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        // Create a new entity to host our line.
        entity = app.entities.create();

        // Attach a new Line component to the entity:
        auto& line = app.entities.emplace<Line>(entity);

        // Set a reference point. This should be near your geometry, and will
        // act as an anchor point for localizing geometry. It will also allow
        // you to set the line's position in geocoordinates.
        line.referencePoint = GeoPoint(SRS::WGS84, -90.0, -20.0);

        const double alt = 10;
        for (double lon = -180; lon <= 0.0; lon += 0.25)
        {
            line.points.emplace_back(lon, -20.0, alt);
        }

        // Create a style that we can change dynamically:
        line.style = LineStyle();
        line.style->color = vsg::vec4{ 1,1,0,1 };
        line.style->width = 3.0f;
        line.style->stipple_pattern = 0xffff;
        line.style->stipple_factor = 1;
        line.write_depth = true;
    }

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        static bool visible = true;
        if (ImGuiLTable::Checkbox("Visible", &visible))
            ecs::setVisible(app.registry, entity, visible);

        auto& component = app.entities.get<Line>(entity);

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
        // Create a new entity to host our line.
        entity = app.entities.create();

        // Attach a line component to our new entity:
        auto& line = app.entities.emplace<Line>(entity);

        // Create the line geometry, which will be relative to a transform.
        const double size = 500000;
        line.points = {
            vsg::dvec3{-size, -size, 0.0},
            vsg::dvec3{ size, -size, 0.0},
            vsg::dvec3{  0.0,  size, 0.0},
            vsg::dvec3{-size, -size, 0.0} };

        // Make a style with color and line width
        line.style = LineStyle{ {1,0,0,1}, 4.0f };
        line.write_depth = true;

        // Add a transform that will place the line on the map
        auto& transform = app.entities.emplace<Transform>(entity);
        transform.setPosition(GeoPoint(SRS::WGS84, -30.0, 10.0, 25000.0));
        transform.node->bound.radius = size; // for horizon culling
    }

    if (ImGuiLTable::Begin("relative linestring"))
    {
        static bool visible = true;
        if (ImGuiLTable::Checkbox("Visible", &visible))
            ecs::setVisible(app.registry, entity, visible);

        auto& line = app.entities.get<Line>(entity);

        if (line.style.has_value())
        {
            if (ImGuiLTable::ColorEdit3("Color", (float*)&line.style->color))
                line.dirty();
        }

        auto& transform = app.entities.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};
