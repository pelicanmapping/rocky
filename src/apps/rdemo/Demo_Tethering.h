/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Viewpoint.h>
#include <rocky/SRS.h>
#include <rocky_vsg/MapManipulator.h>
#include <rocky_vsg/Mesh.h>
#include <rocky_vsg/Line.h>
#include <rocky_vsg/Icon.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

auto Demo_Tethering = [](Application& app)
{
    auto view = app.displayConfiguration.windows.begin()->second.front();
    auto manip = view ? view->getObject<MapManipulator>("rocky.manip") : nullptr;
    if (!manip) return;

    // Make an entity for us to tether to and set it in motion
    static entt::entity entity = entt::null;
    static Status status;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    const float s = 20.0;

    // Make an entity to tether to.
    if (entity == entt::null)
    {
        // Create a host entity:
        entity = app.entities.create();

        // add an icon:
        auto io = app.instance.ioOptions();
        auto image = io.services().readImageFromURI("https://github.com/gwaldron/osgearth/blob/master/data/airport.png?raw=true", io);
        if (image.status.ok()) {
            auto& icon = app.entities.emplace<Icon>(entity);
            icon.image = image.value;
            icon.style = IconStyle{ 48.0f, 0.0f }; // pixels, rotation(rad)
        }

        // add a mesh plane:
        auto& mesh = app.entities.emplace<Mesh>(entity);
        vsg::vec3 verts[4] = { { -s, -s, 0 }, {  s, -s, 0 }, {  s,  s, 0 }, { -s,  s, 0 } };
        unsigned indices[6] = { 0,1,2, 0,2,3 };
        vsg::vec4 color{ 1, 1, 0, 0.55 };
        for (unsigned i = 0; i < 6; ) {
            mesh.add({
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });
        }
        
        // add an arrow line:
        auto& arrow = app.entities.emplace<Line>(entity);
        std::vector<vsg::vec3> v0 { {0,0,0}, {s * 2, 0, 0} };
        std::vector<vsg::vec3> v1{ {s * 1.5, s * 0.5, 0}, {s * 2, 0, 0}, {s * 1.5, -s * 0.5, 0} };
        arrow.push(v0.begin(), v0.end());
        arrow.push(v1.begin(), v1.end());
        arrow.style = LineStyle{ {1,0.5,0,1}, 4.0f };

        // Add a transform:
        auto& xform = app.entities.emplace<EntityTransform>(entity);
        xform.node->setPosition(GeoPoint(SRS::WGS84, -121, 55, 50000));

        // Add a motion component to animate the entity:
        auto& motion = app.entities.emplace<EntityMotion>(entity);
        motion.velocity = { 1000, 0, 0 };
        motion.acceleration = { 0, 0, 0 };
    }

    if (ImGuiLTable::Begin("tethering"))
    {
        bool tethering = manip->isTethering();
        if (ImGuiLTable::Checkbox("Tether active:", &tethering))
        {
            if (tethering)
            {
                auto& xform = app.entities.get<EntityTransform>(entity);
                auto vp = manip->getViewpoint();
                vp.target = PositionedObjectAdapter<GeoTransform>::create(xform.node);
                vp.range = s * 12.0;
                vp.pitch = -45;
                vp.heading = 45;
                manip->setViewpoint(vp, 2.0s);
            }
            else
            {
                manip->home();
            }
        }

        auto& motion = app.entities.get<EntityMotion>(entity);
        ImGuiLTable::SliderDouble("Speed", &motion.velocity.x, 0.0, 10000.0, "%.0lf");
        ImGuiLTable::SliderDouble("Acceleration", &motion.acceleration.x, -100.0, 100.0, "%.1lf");

        ImGuiLTable::End();
    }
};
