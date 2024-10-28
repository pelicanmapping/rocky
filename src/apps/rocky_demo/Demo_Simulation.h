/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>
#include <rocky/vsg/ECS.h>
#include <set>
#include <random>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

namespace
{
    // Simple simulation system runinng in its own thread.
    // It uses a MotionSystem to process Motion components and update
    // their corresponding Transform components.
    class Simulator
    {
    public:
        Application& app;
        EntityMotionSystem motion;

        Simulator(Application& in_app) : app(in_app), motion(in_app.entities) { }

        void run()
        {
            jobs::context context;
            context.pool = jobs::get_pool("simulation");
            jobs::dispatch([this]()
                {
                    while (app.active())
                    {
                        motion.update(app.runtime());
                        std::this_thread::sleep_for(16ms);
                    }
                }, context);
        }
    };
}

auto Demo_Simulation = [](Application& app)
{
    // Make an entity for us to tether to and set it in motion
    static std::set<entt::entity> platforms;
    static Status status;
    static Simulator sim(app);
    const unsigned num_platforms = 10000;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed!");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.message.c_str());
        return;
    }

    const float s = 20.0;

    if (platforms.empty())
    {
        // add an icon:
        auto io = app.instance.io();
        auto image = io.services.readImageFromURI("https://github.com/gwaldron/osgearth/blob/master/data/airport.png?raw=true", io);
        status = image.status;
        if (image.status.ok())
        {
            std::mt19937 mt;
            std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

            auto ll_to_ecef = SRS::WGS84.to(SRS::ECEF);

            for (unsigned i = 0; i < num_platforms; ++i)
            {
                // Create a host entity:
                auto entity = app.entities.create();
                if (image.status.ok())
                {
                    // attach an icon to the host:
                    auto& icon = app.entities.emplace<Icon>(entity);
                    icon.image = image.value;
                    icon.style = IconStyle{ 16.0f + rand_unit(mt) * 16.0f, 0.0f }; // pixels, rotation(rad)
                }

                double lat = -80.0 + rand_unit(mt) * 160.0;
                double lon = -180 + rand_unit(mt) * 360.0;
                double alt = 1000.0 + rand_unit(mt) * 150000.0;
                GeoPoint pos(SRS::WGS84, lon, lat, alt);

                // This is optional, since a Transform can take a point expresssed in any SRS.
                // But since we KNOW the map is geocentric, this will give us better performance
                // by avoiding a conversion every frame.
                pos.transformInPlace(SRS::ECEF);

                // Add a transform component:
                auto& xform = app.entities.emplace<Transform>(entity);
                xform.setPosition(pos);

                // Add a motion component to represent movement.
                auto& motion = app.entities.emplace<Motion>(entity);
                motion.velocity = { -75000 + rand_unit(mt) * 150000, 0.0, 0.0 };

                // Label the platform.
                auto& label = app.entities.emplace<Label>(entity);
                label.text = std::to_string(i);
                label.style.font = app.runtime().defaultFont;
                label.style.pointSize = 16.0f;
                label.style.outlineSize = 0.2f;

                platforms.emplace(entity);
            }

            sim.run();
        }
    }

    ImGui::Text("Simulating %d platforms", platforms.size());
};
