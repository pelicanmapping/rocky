/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Line.h>

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
        // act as an anchor point for localizing geometry.
        // Use the returned SRSOperation to transform points for use in your line.
        //GeoPoint refPoint(SRS::WGS84, -90.0, -20.0);
        GeoPoint refPoint(SRS::WGS84, 50.103129, 26.327247);
        auto xform = line.setReferencePoint(refPoint);

        const double alt = 10;
        std::vector<glm::dvec3> points;
        //for (double lon = -180; lon <= 0.0; lon += 0.25)
        //{
        //    glm::dvec3 point;
        //    if (xform(glm::dvec3(lon, -20.0, alt), point))
        //        points.emplace_back(point);
        //}

        glm::dvec3 point;
        xform(glm::dvec3(50.103129, 26.327247, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1031457750325, 26.327230655112, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1035726159418, 26.3268494593497, 9.99999999999999), point); points.push_back(point);
        xform(glm::dvec3(50.1036057346175, 26.3268228050053, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1036388532965, 26.326799548534, 9.99999999999999), point); points.push_back(point);
        xform(glm::dvec3(50.1036719720074, 26.3267813029714, 9.99999999999997), point); points.push_back(point);
        xform(glm::dvec3(50.1037050909888, 26.3267688989936, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1037382120141, 26.3267614727557, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1037713469226, 26.3267561301391, 9.99999999999998), point); points.push_back(point);
        xform(glm::dvec3(50.1038045675359, 26.3267494039326, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038382733454, 26.3267404887731, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038745178198, 26.3267243706194, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038745178198, 26.3267243706194, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038872827415, 26.3267427624762, 12.1729600690942), point); points.push_back(point);
        xform(glm::dvec3(50.1039133194297, 26.326774132706, 14.2043353140336), point); points.push_back(point);
        xform(glm::dvec3(50.1039442789989, 26.3268044700009, 14.8654013623941), point); points.push_back(point);
        xform(glm::dvec3(50.1039799152304, 26.3268279774877, 14.9826782408394), point); points.push_back(point);
        xform(glm::dvec3(50.1040173973713, 26.3268427311765, 14.9477806954852), point); points.push_back(point);
        xform(glm::dvec3(50.1040552250293, 26.326850946745, 14.6713950639817), point); points.push_back(point);
        xform(glm::dvec3(50.1040930247916, 26.326858386002, 13.8206358812665), point); points.push_back(point);
        xform(glm::dvec3(50.1041302982697, 26.3268673436876, 12.4419508088357), point); points.push_back(point);
        xform(glm::dvec3(50.1041639450319, 26.3268773721128, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1041639450319, 26.3268773721128, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1043066117892, 26.3269527903529, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1043066117892, 26.3269527903529, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1039558774553, 26.3269533741567, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1039182921164, 26.3269544801621, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038807067775, 26.3269565834256, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1038431214387, 26.3269596698877, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1036927800831, 26.3269733726499, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1036551947443, 26.3269776045867, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1036176094054, 26.3269835859794, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1035800240665, 26.3269920711013, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1035424387276, 26.3270036534096, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1035048533888, 26.3270184554716, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1034672680499, 26.3270357211518, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1033920973721, 26.3270718381579, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1033169266944, 26.3271052937524, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1032793413555, 26.3271230320447, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1032417560166, 26.3271451275491, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1032041706777, 26.3271738382909, 10), point); points.push_back(point);
        xform(glm::dvec3(50.1031665853388, 26.3272075804958, 10), point); points.push_back(point);
        xform(glm::dvec3(50.103129, 26.327247, 10), point); points.push_back(point);



        line.push(points.begin(), points.end());

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
        // Create a new entity to host our line.
        entity = app.entities.create();

        // Attach a line component to our new entity:
        auto& line = app.entities.emplace<Line>(entity);

        // Create the line geometry, which will be relative to a geolocation
        const double size = 500000;
        std::vector<vsg::vec3> points = {
            {-size, -size, 0},
            { size, -size, 0},
            {    0,  size, 0},
            {-size, -size, 0} };
        line.push(points.begin(), points.end());

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
        auto& line = app.entities.get<Line>(entity);

        ImGuiLTable::Checkbox("Visible", &line.active);

        if (line.style.has_value())
        {
            if (ImGuiLTable::ColorEdit3("Color", (float*)&line.style->color))
                line.dirty();
        }

        auto& transform = app.entities.get<Transform>(entity);
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
