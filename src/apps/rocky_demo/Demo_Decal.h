/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

auto Demo_Decal = [](Application& app)
{
    static entt::entity e_decal = entt::null;
    static entt::entity e_normal = entt::null;
    static entt::entity e_texture_style = entt::null;
    static double scale = 10000.0;
    static CallbackSubs subs;
    static bool moveWithMouse = false;
    static glm::vec3 normal = glm::vec3(0, 0, 1);
    static Image::Ptr image = {};

    if (e_decal == entt::null)
    {
        // load up a texture image
        auto& io = app.io();
        auto r = io.services().readImageFromURI("https://readymap.org/readymap/filemanager/download/public/icons/placemark32.png", io);
        if (r.ok())
        {
            image = r.value();
            image->flipVerticalInPlace();            
        }

        auto [_, reg] = app.registry.write();

        // create a decal style that uses the texture image
        e_texture_style = reg.create();
        auto& style = reg.emplace<DecalStyle>(e_texture_style);
        style.image = image;

        // Create our decal. We can assign the style to it later.
        e_decal = reg.create();
        reg.emplace<Decal>(e_decal);

        // A transform component to place and move it on the map
        auto& transform = reg.emplace<Transform>(e_decal);
        transform.position = GeoPoint(SRS::WGS84, 0.0, 51.50, 0.0);
        transform.localMatrix = glm::scale(glm::dmat4(1), glm::dvec3(scale));
        transform.topocentric = true;

        // normal vector for the decal (used to orient it)
        e_normal = reg.create();
        auto& lineStyle = reg.emplace<LineStyle>(e_normal);
        lineStyle.color = StockColor::Cyan;
        lineStyle.width = 4.0f;
        auto& lineGeom = reg.emplace<LineGeometry>(e_normal);
        lineGeom.topology = LineTopology::Segments;
        lineGeom.points = { {0, 0, 0}, {0, 0, 1} };
        reg.emplace<Line>(e_normal, lineGeom, lineStyle);

        auto lineXform = reg.emplace<Transform>(e_normal);
        lineXform.position = transform.position;
        lineXform.localMatrix = transform.localMatrix;
        lineXform.topocentric = transform.topocentric;

        // install mouse callback that will reposition the decal under the mouse as it moves:
        auto handler = app.viewer->getObject<GeoMouseHandler>("demo.mouse");
        subs += handler->onMouseMove([&](const TerrainIntersection& i, const View& view)
            {
                if (moveWithMouse && i.point)
                {
                    app.registry.write([&](entt::registry& r)
                        {
                            auto& transform = r.get<Transform>(e_decal);
                            transform.position = i.point;
                            transform.dirty(r);

                            // transform the intersection normal into local basis space 
                            // and use it to rotate our normal vector line
                            auto local2world = i.point.srs.topocentricToWorldMatrix(i.point);
                            auto world2local = glm::inverse(local2world);
                            normal = glm::dmat3(world2local) * i.normal;
                            auto& normalXform = r.get<Transform>(e_normal);
                            normalXform.position = transform.position;
                            normalXform.topocentric = transform.topocentric;
                            auto q = glm::normalize(glm::dquat(1.0f + normal.z, -normal.y, normal.x, 0.0f));
                            normalXform.localMatrix = glm::mat4_cast(q) * transform.localMatrix;
                            normalXform.dirty(r);
                        });
                }
            });

        app.vsgcontext->requestFrame();
    }

    auto reader = app.registry.read();
    auto& reg = reader.registry;
    auto& decal = reg.get<Decal>(e_decal);

    if (ImGuiLTable::Begin("decal"))
    {
        auto& v = reg.get<Visibility>(e_decal).visible[0];
        if (ImGuiLTable::Checkbox("Show", &v))
        {
            reg.get<Visibility>(e_decal).visible = v;
            reg.get<Visibility>(e_normal).visible = v;
        }

        auto& transform = reg.get<Transform>(e_decal);
        auto& lineXform = reg.get<Transform>(e_normal);

        auto sync = [&]() {
            lineXform.position = transform.position;
            lineXform.localMatrix = transform.localMatrix;
            lineXform.topocentric = transform.topocentric;
            auto q = glm::normalize(glm::dquat(1.0f + normal.z, -normal.y, normal.x, 0.0f));
            lineXform.localMatrix = glm::mat4_cast(q) * transform.localMatrix;
            lineXform.dirty(reg);
        };

        auto rot = quaternion_from_matrix<glm::dquat>(transform.localMatrix);
        auto [pitch, roll, heading] = euler_degrees_from_quaternion(rot);

        ImGuiLTable::Checkbox("Move with mouse", &moveWithMouse);

        if (image)
        {
            static bool useTexture = false;
            if (ImGuiLTable::Checkbox("Texture", &useTexture))
            {
                decal.style = useTexture ? e_texture_style : entt::null;
                decal.dirty(reg);
            }
        }

        if (!moveWithMouse)
        {
            if (ImGuiLTable::SliderDouble("Heading", &heading, -180.0, 180.0, "%.1lf"))
            {
                auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
                sync();
            }

            if (ImGuiLTable::SliderDouble("Pitch", &pitch, -90.0, 90.0, "%.1lf"))
            {
                auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
                sync();
            }

            if (ImGuiLTable::SliderDouble("Roll", &roll, -90.0, 90.0, "%.1lf"))
            {
                auto rot = quaternion_from_euler_degrees(pitch, roll, heading);
                transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
                transform.dirty(reg);
                sync();
            }
        }

        if (ImGuiLTable::SliderDouble("Scale", &scale, 1.0, 100000.0, "%.1lf", ImGuiSliderFlags_Logarithmic))
        {
            transform.localMatrix = glm::mat4_cast(rot) * glm::scale(glm::dmat4(1), glm::dvec3(scale));
            transform.dirty(reg);
            sync();
        }

        ImGuiLTable::End();
    }
};
