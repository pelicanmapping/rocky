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
        app.registry.write([&](entt::registry& r)
            {
                // Create a new entity to host our line.
                entity = r.create();

                // Build the geometry.
                auto& geometry = r.emplace<LineGeometry>(entity);

                // Let's transform geodetic (long, lat) points into our world SRS:
                auto xform = SRS::WGS84.to(app.mapNode->srs());
                for (double lon = -180; lon <= 0.0; lon += 0.25)
                {
                    auto world = xform(glm::dvec3(lon, 20.0, 0.0));
                    geometry.points.emplace_back(world);
                }

                // Style our line.
                auto& style = r.emplace<LineStyle>(entity);
                style.color = Color::Yellow;
                style.width = 3.0f;
                style.depthOffset = 1000.0f;

                // A "Line" renders the given geometry with the given style.
                auto& line = r.emplace<Line>(entity, geometry, style);

                app.vsgcontext->requestFrame();
            });
    }

    if (ImGuiLTable::Begin("absolute linestring"))
    {
        auto [lock, r] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(r, entity, visible);

        auto& style = r.get<LineStyle>(entity);

        float* col = (float*)&style.color;
        if (ImGuiLTable::ColorEdit3("Color", col))
            style.dirty(r);

        if (ImGuiLTable::SliderFloat("Width", &style.width, 1.0f, 15.0f, "%.0f"))
            style.dirty(r);

        if (ImGuiLTable::SliderInt("Stipple pattern", &style.stipplePattern, 0x0001, 0xffff, "%04x", ImGuiSliderFlags_Logarithmic))
            style.dirty(r);

        if (ImGuiLTable::SliderInt("Stipple factor", &style.stippleFactor, 1, 4))
            style.dirty(r);


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

        // Create the line geometry, which will be relative to a transform.
        auto& geometry = registry.emplace<LineGeometry>(entity);
        const double size = 500000;
        geometry.points = {
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{  0.0,  size, 0.0},
            glm::dvec3{-size, -size, 0.0} };

        // Make a style with color and line width
        auto& style = registry.emplace<LineStyle>(entity);
        style.color = Color::Red;

        // Attach a line component to our new entity:
        auto& line = registry.emplace<Line>(entity, geometry, style);

        // Add a transform that will place the line on the map
        auto& transform = registry.emplace<Transform>(entity);
        transform.topocentric = true;
        transform.position = GeoPoint(SRS::WGS84, -30.0, 10.0, 25000.0);
        transform.radius = size; // for culling

        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("relative linestring"))
    {
        auto [_, r] = app.registry.read();

        static bool visible = true;
        if (ImGuiLTable::Checkbox("Show", &visible))
            setVisible(r, entity, visible);

        auto& style = r.get<LineStyle>(entity);

        if (ImGuiLTable::ColorEdit3("Color", (float*)&style.color))
            style.dirty(r);

        auto& transform = r.get<Transform>(entity);

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
    static std::array<entt::entity, 3> styles;
    static std::array<entt::entity, 3> geoms;
    static std::vector<entt::entity> entities;
    static bool visible = true;
    static bool regenerate = false;
    const unsigned count = 10000;

    if (regenerate)
    {
        app.registry.write([&](entt::registry& registry)
            {
                registry.destroy(entities.begin(), entities.end());
                entities.clear();
                regenerate = false;
            });
    }

    if (entities.empty())
    {
        auto [_, registry] = app.registry.write();

        const double size = 100000;

        // One style that all Lines will share:
        styles[0] = entities.emplace_back(registry.create());
        LineStyle& style0 = registry.emplace<LineStyle>(styles[0]);
        style0.color = Color::Red;
        style0.width = 2.0f;

        styles[1] = entities.emplace_back(registry.create());
        LineStyle& style1 = registry.emplace<LineStyle>(styles[1]);
        style1.color = Color::Yellow;
        style1.width = 2.0f;
        
        styles[2] = entities.emplace_back(registry.create());
        LineStyle& style2 = registry.emplace<LineStyle>(styles[2]);
        style2.color = Color::Lime;
        style2.width = 2.0f;

        // Create a few different line objects.
        geoms[0] = entities.emplace_back(registry.create());
        auto& square = registry.emplace<LineGeometry>(geoms[0]);
        square.points = {
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{ size,  size, 0.0},
            glm::dvec3{-size,  size, 0.0},
            glm::dvec3{-size, -size, 0.0} };

        geoms[1] = entities.emplace_back(registry.create());
        auto& triangle = registry.emplace<LineGeometry>(geoms[1]);
        triangle.points = {
            glm::dvec3{0.0,  size, 0.0},
            glm::dvec3{ size, -size, 0.0},
            glm::dvec3{-size, -size, 0.0},
            glm::dvec3{0.0,  size, 0.0} };

        geoms[2] = entities.emplace_back(registry.create());
        auto& circle = registry.emplace<LineGeometry>(geoms[2]);
        const int circle_points = 64;
        circle.points.reserve(circle_points);
        for (int i = 0; i <= circle_points; ++i) {
            double angle = (double)i / (double)circle_points * glm::two_pi<double>();
            circle.points.emplace_back(glm::dvec3{ cos(angle) * size, sin(angle) * size, 0.0 });
        }

        // Now create a bunch of entities, each of which shares one of the above line objects.
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

        entities.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            auto e = entities.emplace_back(registry.create());

            registry.emplace<Line>(e, geoms[i % 3], styles[i % 3]);

            double lat = rand_unit(mt) * 170.0 - 85.0;
            double lon = rand_unit(mt) * 360.0 - 180.0;

            // Add a transform that will place the line on the map
            auto& transform = registry.emplace<Transform>(e);
            transform.topocentric = true;
            transform.position = GeoPoint(SRS::WGS84, lon, lat, 25000.0);
            transform.radius = size; // for culling

            // Decluttering object, just to prove that it works with shared geometies:
            auto& dc = registry.emplace<Declutter>(e);
            dc.rect = { -10, -10, 10, 10 };
            dc.priority = i % 3;
        }

        app.vsgcontext->requestFrame();
    }

    ImGui::TextWrapped("Sharing: %d Line instances share LineStyle and LineGeometry components, but each has its own Transform.", count);

    if (ImGuiLTable::Begin("instanced linestring"))
    {
        auto [_, r] = app.registry.read();

        auto& style0 = r.get<LineStyle>(styles[0]);
        if (ImGuiLTable::ColorEdit3("Color 1", (float*)&style0.color))
            style0.dirty(r);
        auto& style1 = r.get<LineStyle>(styles[1]);
        if (ImGuiLTable::ColorEdit3("Color 2", (float*)&style1.color))
            style1.dirty(r);
        auto& style2 = r.get<LineStyle>(styles[2]);
        if (ImGuiLTable::ColorEdit3("Color 3", (float*)&style2.color))
            style2.dirty(r);

        if (ImGuiLTable::Button("Regenerate"))
        {
            regenerate = true;
        }

        ImGuiLTable::End();
    }
};
