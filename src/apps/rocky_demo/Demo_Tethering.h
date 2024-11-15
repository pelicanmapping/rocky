/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Viewpoint.h>
#include <rocky/SRS.h>
#include <rocky/vsg/MapManipulator.h>
#include <rocky/vsg/Mesh.h>
#include <rocky/vsg/Line.h>
#include <rocky/vsg/Icon.h>
#include <rocky/vsg/Transform.h>
#include <rocky/vsg/Motion.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

auto Demo_Tethering = [](Application& app)
{
    auto view = app.displayManager->windowsAndViews.begin()->second.front();
    
    auto manip = MapManipulator::get(view);
    if (!manip)
        return;

    // Make an entity for us to tether to and set it in motion
    static entt::entity entity = entt::null;
    static Status status;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    const double s = 20.0;

    // Make an entity to tether to.
    if (entity == entt::null)
    {
        // Create a host entity:
        entity = app.registry.create();

        // add an icon:
        auto io = app.instance.io();
        auto image = io.services.readImageFromURI("https://github.com/gwaldron/osgearth/blob/master/data/airport.png?raw=true", io);
        if (image.status.ok())
        {
            auto& icon = app.registry.emplace<Icon>(entity);
            icon.image = image.value;
            icon.style = IconStyle{ 48.0f, 0.0f }; // pixels, rotation(rad)
        }

        // add a mesh plane:
        auto& mesh = app.registry.emplace<Mesh>(entity);
        vsg::dvec3 verts[4] = { { -s, -s, 0 }, {  s, -s, 0 }, {  s,  s, 0 }, { -s,  s, 0 } };
        unsigned indices[6] = { 0,1,2, 0,2,3 };
        vsg::vec4 color{ 1, 1, 0, 0.55f };
        for (unsigned i = 0; i < 6; )
        {
            mesh.triangles.emplace_back(Triangle{
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });
        }

        // add an arrow line:
        auto& arrow = app.registry.emplace<Line>(entity);
        arrow.points = { 
            vsg::dvec3{ s * 1.5, s * 0.5, 0.0 },
            vsg::dvec3{ s * 2.0, 0.0, 0.0 },
            vsg::dvec3{ s * 2.0, 0.0, 0.0 },
            vsg::dvec3{ s * 1.5, -s * 0.5, 0.0 },
            vsg::dvec3{ s * 2.0, 0.0, 0.0 },
            vsg::dvec3{ s * 0.0, 0.0, 0.0 }
        };

        arrow.style = LineStyle{ {1,0.5,0,1}, 4.0f };
        arrow.topology = Line::Topology::Segments;

        // Add a transform:
        auto& xform = app.registry.emplace<Transform>(entity);
        xform.setPosition(GeoPoint(SRS::WGS84, -121, 55, 50000));

        // Add a motion component to animate the entity:
        auto& motion = app.registry.emplace<Motion>(entity);
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
                auto& xform = app.registry.get<Transform>(entity);
                auto vp = manip->getViewpoint();
                //vp.target = PositionedObjectAdapter<GeoTransform>::create(xform.node);
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

        auto& motion = app.registry.get<Motion>(entity);
        ImGuiLTable::SliderDouble("Speed", &motion.velocity.x, 0.0, 10000.0, "%.0lf");
        ImGuiLTable::SliderDouble("Acceleration", &motion.acceleration.x, -100.0, 100.0, "%.1lf");

        ImGuiLTable::End();
    }
};
