/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;


auto Demo_OrderedRendering = [](Application& app)
{
    static entt::entity cube = entt::null;
    static std::vector<entt::entity> lines;

    if (cube == entt::null)
    {
        app.registry.write([&](entt::registry& reg)
            {
                const double s = 250000.0;

                cube = reg.create();
                {
                    auto& geom = reg.emplace<MeshGeometry>(cube);

                    glm::dvec3 vertices[8] = {
                        { -s, -s, -s }, {  s, -s, -s },
                        {  s,  s, -s }, { -s,  s, -s },
                        { -s, -s,  s }, {  s, -s,  s },
                        {  s,  s,  s }, { -s,  s,  s }
                    };

                    unsigned indices[48] = {
                        0,3,2, 0,2,1, 4,5,6, 4,6,7,
                        1,2,6, 1,6,5, 3,0,4, 3,4,7,
                        0,1,5, 0,5,4, 2,3,7, 2,7,6
                    };

                    Color color{ 0, 1, 1, 0.5f };
                    geom.vertices.reserve(12 * 3);
                    geom.colors.reserve(12 * 3);

                    for (int i = 0; i < 48; )
                    {
                        for (int v = 0; v < 3; ++v)
                        {
                            geom.indices.emplace_back(i);
                            geom.vertices.emplace_back(vertices[indices[i++]]);
                            geom.colors.emplace_back(color);
                        }

                        if ((i % 6) == 0)
                            color.g *= 0.8f, color.b *= 0.8f;
                    }

                    // Set up our style to use the embedded colors.
                    auto& style = reg.emplace<MeshStyle>(cube);
                    style.useGeometryColors = true;
                    style.twoPassAlpha = true;
                    style.transparencyBin = true;
                    style.drawBackfaces = true;

                    for (int i = 0; i < 5; ++i)
                    {
                        auto e = reg.create();
                        auto& mesh = reg.emplace<Mesh>(e, geom, style);
                        auto& xform = reg.emplace<Transform>(e);
                        xform.topocentric = true;
                        xform.position = GeoPoint(SRS::WGS84, -90 + (double)(7 * i), 0.0, s * 3.0);
                    }
                }

                {
                    auto e = reg.create();

                    auto& geom = reg.emplace<LineGeometry>(e);
                    geom.topology = LineTopology::Segments;
                    geom.points.emplace_back(-s * 1.5, 0, 0);
                    geom.points.emplace_back(s * 1.5, 0, 0);
                    geom.points.emplace_back(0, -s * 1.5, 0);
                    geom.points.emplace_back(0, s * 1.5, 0);
                    geom.points.emplace_back(0, 0, -s * 1.5);
                    geom.points.emplace_back(0, 0, s * 1.5);

                    auto& style = reg.emplace<LineStyle>(e);
                    style.color = StockColor::Yellow;
                    style.width = 5.0f;

                    for (int i = 0; i < 5; ++i)
                    {
                        auto e2 = lines.emplace_back(reg.create());
                        auto& line = reg.emplace<Line>(e2, geom, style);
                        auto& xform = reg.emplace<Transform>(e2);
                        xform.topocentric = true;
                        xform.position = GeoPoint(SRS::WGS84, -90.0 + (double)(7 * i), 0.0, s * 3.0);
                    }
                }
            });
    }

    ImGui::TextWrapped("%s", "Test: ordered rendering");

    if (ImGuiLTable::Begin("ordered rendering demo"))
    {
        app.registry.read([&](entt::registry& reg)
            {
                auto& style = reg.get<MeshStyle>(cube);

                if (ImGuiLTable::Checkbox("Cubes: Draw backfaces", &style.drawBackfaces))
                    style.dirty(reg);

                if (ImGuiLTable::Checkbox("Cubes: Write depth", &style.writeDepth))
                    style.dirty(reg);

                if (ImGuiLTable::Checkbox("Cubes: Two-pass alpha", &style.twoPassAlpha))
                    style.dirty(reg);

                if (ImGuiLTable::Checkbox("Cubes: Transparency bin", &style.transparencyBin))
                    style.dirty(reg);

                bool linesVisible = reg.get<Visibility>(lines.front()).visible[0];
                if (ImGuiLTable::Checkbox("Lines: show", &linesVisible))
                {
                    for (auto& line : lines) {
                        auto& v = reg.get<Visibility>(line);
                        setVisible(reg, line, linesVisible);
                    }
                }
            });
        ImGuiLTable::End();
    }
};
