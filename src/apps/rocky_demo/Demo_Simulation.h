/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Icon.h>
#include <set>
#include <random>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

auto Demo_Simulation = [](Application& app)
{
    // Make an entity for us to tether to and set it in motion
    static std::set<entt::entity> platforms;
    static Status status;
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

            for (unsigned i = 0; i < num_platforms; ++i)
            {
                // Create a host entity:
                auto entity = app.entities.create();
                if (image.status.ok())
                {
                    // attach an icon to the host:
                    auto& icon = app.entities.emplace<Icon>(entity);
                    icon.image = image.value->clone();
                    icon.style = IconStyle{ 16.0f + rand_unit(mt) * 16.0f, 0.0f }; // pixels, rotation(rad)
                }

                // Add a transform:
                double lat = -80.0 + rand_unit(mt) * 160.0;
                double lon = -180 + rand_unit(mt) * 360.0;
                double alt = 20000.0 + rand_unit(mt) * 20000.0;
                auto& xform = app.entities.emplace<Transform>(entity);
                xform.setPosition(GeoPoint(SRS::WGS84, lon, lat, alt));

                // Add a motion component to animate the entity:
                auto& motion = app.entities.emplace<Motion>(entity);
                motion.velocity = { -75000 + rand_unit(mt) * 150000, 0.0, 0.0 };

                platforms.emplace(entity);
            }
        }
    }

    ImGui::Text("Simulating %d platforms", platforms.size());
};
