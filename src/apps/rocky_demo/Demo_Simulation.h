/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>
#include <rocky/vsg/Transform.h>
#include <rocky/vsg/Motion.h>
#include <rocky/vsg/DisplayManager.h>
#include <set>
#include <random>
#include <rocky/rtree.h>

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
        MotionSystem motion;
        float sim_hertz = 5.0f; // updates per second

        Simulator(Application& in_app) :
            app(in_app),
            motion(in_app.registry) { }

        void run()
        {
            app.backgroundServices.start("rocky::simulation",
                [this](jobs::cancelable& token)
                {
                    Log()->info("Simulation thread starting.");
                    while (!token.canceled())
                    {
                        run_at_frequency f(sim_hertz);
                        motion.update(app.runtime());
                        app.runtime().requestFrame();
                    }
                    Log()->info("Simulation thread terminating.");
                });
        }
    };
}

auto Demo_Simulation = [](Application& app)
{
    const char* icon_location = "https://readymap.org/readymap/filemanager/download/public/icons/airport.png";

    // Make an entity for us to tether to and set it in motion
    static std::set<entt::entity> platforms;
    static Status status;
    static Simulator sim(app);
    const unsigned num_platforms = 2500;

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
        auto image = io.services.readImageFromURI(icon_location, io);
        status = image.status;

        if (image.status.ok())
        {
            std::mt19937 mt;
            std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

            auto ll_to_ecef = SRS::WGS84.to(SRS::ECEF);

            for (unsigned i = 0; i < num_platforms; ++i)
            {
                float t = (float)i / (float)(num_platforms);

                // Create a host entity:
                auto entity = app.entities.create();

                // Attach an icon:
                auto& icon = app.entities.emplace<Icon>(entity);
                icon.style = IconStyle{ 16.0f + t*16.0f, 0.0f }; // pixels, rotation(rad)

                if (image.status.ok())
                    icon.image = image.value;

                double lat = -80.0 + rand_unit(mt) * 160.0;
                double lon = -180 + rand_unit(mt) * 360.0;
                double alt = 1000.0 + t * 1000000.0;
                GeoPoint pos(SRS::WGS84, lon, lat, alt);

                // This is optional, since a Transform can take a point expresssed in any SRS.
                // But since we KNOW the map is geocentric, this will give us better performance
                // by avoiding a conversion every frame.
                pos.transformInPlace(SRS::ECEF);

                // Add a transform component:
                auto& transform = app.entities.emplace<Transform>(entity);
                transform.localTangentPlane = false;
                transform.setPosition(pos);

                // Add a motion component to represent movement:
                double initial_bearing = -180.0 + rand_unit(mt) * 360.0;
                auto& motion = app.entities.emplace<MotionGreatCircle>(entity);
                motion.velocity = { -75000 + rand_unit(mt) * 150000, 0.0, 0.0 };
                motion.normalAxis = pos.srs.ellipsoid().greatCircleRotationAxis(glm::dvec3(lon, lat, 0.0), initial_bearing);

                // Place a label below the platform:
                auto& label = app.entities.emplace<Label>(entity);
                label.text = std::to_string(i);
                label.style.font = app.runtime().defaultFont;
                label.style.pointSize = 16.0f + t * 5.0f;
                label.style.outlineSize = 0.5f;
                label.style.pixelOffset.y = -icon.style.size_pixels * 0.5f - 5.0f;
                label.style.verticalAlignment = vsg::StandardLayout::TOP_ALIGNMENT;

                platforms.emplace(entity);
            }

            sim.run();
        }
    }

    ImGui::Text("Simulating %ld platforms", platforms.size());
    if (ImGuiLTable::Begin("sim"))
    {
        ImGuiLTable::SliderFloat("Update rate", &sim.sim_hertz, 1.0f, 120.0f, "%.0f hz");

        ImGuiLTable::End();
    }
};
