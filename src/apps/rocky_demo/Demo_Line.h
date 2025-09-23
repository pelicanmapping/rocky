/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <random>

using namespace ROCKY_NAMESPACE;

auto Demo_Line_Absolute = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        // Create a new entity to host our line.
        entity = registry.create();

        // Attach a new Line component to the entity:
        auto& line = registry.emplace<Line>(entity);

        // Let's transform geodetic (long, lat) points into our world SRS:
        auto xform = SRS::WGS84.to(app.mapNode->srs());

        for (double lon = -180; lon <= 0.0; lon += 0.25)
        {
            auto world = xform(glm::dvec3(lon, 20.0, 0.0));
            line.points.emplace_back(world);
        }

        // Create a style that we can change dynamically:
        line.style.color = Color::Yellow;
        line.style.width = 3.0f;
        line.style.depth_offset = 1000.0f;

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        auto [lock, registry] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(registry, entity, visible);

        auto& line = registry.get<Line>(entity);

        float* col = (float*)&line.style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
            line.dirty();

        if (ImGuiLTable::SliderFloat("Width", &line.style.width, 1.0f, 15.0f, "%.0f"))
            line.dirty();

        if (ImGuiLTable::SliderInt("Stipple pattern", &line.style.stipple_pattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
            line.dirty();

        if (ImGuiLTable::SliderInt("Stipple factor", &line.style.stipple_factor, 1, 4))
            line.dirty();

        ImGuiLTable::End();
    }
};

auto Demo_Line_Relative = [](Application& app)
{
    static entt::entity entity = entt::null;
    static bool visible = true;

    if (entity == entt::null)
    {
        auto [lock, registry] = app.registry.write();

        // Create a new entity to host our line.
        entity = registry.create();

        // Attach a line component to our new entity:
        auto& line = registry.emplace<Line>(entity);

        // Create the line geometry, which will be relative to a transform.
        const double size = 500000;
        line.points = {
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{  0.0,  size, 0.0},
            glm::dvec3{-size, -size, 0.0} };

        // Make a style with color and line width
        line.style = LineStyle{ {1,0,0,1}, 4.0f };
        line.writeDepth = true;

        // Add a transform that will place the line on the map
        auto& transform = registry.emplace<Transform>(entity);
        transform.topocentric = true;
        transform.position = GeoPoint(SRS::WGS84, -30.0, 10.0, 25000.0);
        transform.radius = size; // for culling

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("relative linestring"))
    {
        auto [lock, registry] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(registry, entity, visible);

        auto& line = registry.get<Line>(entity);

        if (ImGuiLTable::ColorEdit3("Color", (float*)&line.style.color))
            line.dirty();

        auto& transform = registry.get<Transform>(entity);

        if (ImGuiLTable::SliderDouble("Latitude", &transform.position.y, -85.0, 85.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Longitude", &transform.position.x, -180.0, 180.0, "%.1lf"))
            transform.dirty();

        if (ImGuiLTable::SliderDouble("Altitude", &transform.position.z, 0.0, 2500000.0, "%.1lf"))
            transform.dirty();

        ImGuiLTable::End();
    }
};

auto Demo_Line_Shared = [](Application& app)
{
    static std::shared_ptr<Line> shared_line1;
    static std::shared_ptr<Line> shared_line2;
    static std::shared_ptr<Line> shared_line3;
    static std::vector<entt::entity> entities;
    static bool visible = true;
    static bool regenerate = false;

    if (regenerate)
    {
        auto [lock, registry] = app.registry.write();
        registry.destroy(entities.begin(), entities.end());
        entities.clear();
        regenerate = false;
    }

    if (entities.empty())
    {
        auto [lock, registry] = app.registry.write();

        const double size = 100000;

        // define the line:
        Line line1; // cyna square
        line1.style.color = Color::Cyan;
        line1.style.width = 2.0f;
        line1.points = {
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{ size,  size, 0.0},
            glm::dvec3{-size,  size, 0.0},
            glm::dvec3{-size, -size, 0.0} };
        line1.topology = Line::Topology::Strip;
        shared_line1 = std::make_shared<Line>(std::move(line1));

        Line line2; // purple triangle
        line2.style.color = Color::Purple;
        line2.style.width = 5.0f;
        line2.points = {
            glm::dvec3{0.0,  size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{0.0,  size, 0.0} };
        shared_line2 = std::make_shared<Line>(std::move(line2));

        Line line3; // yellow circle
        line3.style.color = Color::Yellow;
        line3.style.width = 3.0f;
        const int circle_points = 64;
        for (int i = 0; i <= circle_points; ++i) {
            double angle = (double)i / (double)circle_points * glm::two_pi<double>();
            line3.points.emplace_back(glm::dvec3{ cos(angle) * size, sin(angle) * size, 0.0 });
        }
        shared_line3 = std::make_shared<Line>(std::move(line3));

        const unsigned count = 10000;
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

        entities.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            auto e = registry.create();
            if (i % 3 == 0)
                registry.emplace<SharedComponent<Line>>(e, shared_line1);
            else if (i % 3 == 1)
                registry.emplace<SharedComponent<Line>>(e, shared_line2);
            else
                registry.emplace<SharedComponent<Line>>(e, shared_line3);

            double lat = rand_unit(mt) * 170.0 - 85.0;
            double lon = rand_unit(mt) * 360.0 - 180.0;

            // Add a transform that will place the line on the map
            auto& transform = registry.emplace<Transform>(e);
            transform.topocentric = true;
            transform.position = GeoPoint(SRS::WGS84, lon, lat, 25000.0);
            transform.radius = size; // for culling

            // Decluttering object, just to prove that it works with SharedComponents:
            auto& dc = registry.emplace<Declutter>(e);
            dc.rect = { -10, -10, 10, 10 };
            dc.priority = i % 3;

            entities.emplace_back(e);
        }

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped("Sharing: %d instances share the same Line component, but each has its own Transform.", (int)entities.size());

    if (ImGuiLTable::Begin("instanced linestring"))
    {
        auto [lock, registry] = app.registry.read();

        auto& shared = registry.get<SharedComponent<Line>>(entities.front());
        auto& line = *shared.pointer;

        if (ImGuiLTable::ColorEdit3("Color", (float*)&line.style.color))
        {
            shared.dirty();
        }

        if (ImGuiLTable::Button("Regenerate"))
        {
            regenerate = true;
        }

        ImGuiLTable::End();
    }
};
