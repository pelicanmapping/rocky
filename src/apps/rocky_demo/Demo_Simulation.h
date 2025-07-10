/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs.h>
#include <rocky/vsg/ecs/MotionSystem.h>
#include <rocky/vsg/DisplayManager.h>
#include <rocky/rtree.h>
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
    // 
    // Be careful. A background thread manipulating the Registry must be careful
    // not to starve the rendering thread by write-locking the Registry for too long.
    class Simulator
    {
    public:
        Application& app;
        MotionSystem motion;
        float sim_hertz = 10.0f; // updates per second

        Simulator(Application& in_app) :
            app(in_app),
            motion(in_app.registry) { }

        void run()
        {
            app.background.start("rocky::simulation", [this](jobs::cancelable& token)
                {
                    Log()->info("Simulation thread starting.");
                    while (!token.canceled())
                    {
                        run_at_frequency f(sim_hertz);
                        motion.update(app.vsgcontext);
                        app.vsgcontext->requestFrame();
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
        auto [lock, registry] = app.registry.write();

        // add an icon:
        auto io = app.vsgcontext->io;
        auto image = io.services.readImageFromURI(icon_location, io);
        status = image.status;

        if (image.status.ok())
        {
            std::mt19937 mt;
            std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

            auto render_widget = [](WidgetInstance& i)
                {
                    Transform& t = i.registry.get<Transform>(i.entity);

                    auto point = t.position.transform(SRS::WGS84);

                    i.begin();

                    i.center.x += i.size.x/2 + 12;

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
                    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.35f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.0f));

                    i.render([&]()
                        {
                            ImGui::Text("ID:  %s", i.widget.text.c_str());
                            ImGui::Separator();
                            ImGui::Text("Lat: %.2f", point.y);
                            ImGui::Separator();
                            ImGui::Text("Lon: %.2f", point.x);
                            ImGui::Separator();
                            ImGui::Text("Alt: %.0f", point.z);
                        });

                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();

                    i.end();

                    // update decluttering volume
                    auto& dc = i.registry.get<Declutter>(i.entity);
                    dc.rect = Rect(i.size.x, i.size.y);
                };

            auto ll_to_ecef = SRS::WGS84.to(SRS::ECEF);

            for (unsigned i = 0; i < num_platforms; ++i)
            {
                float t = (float)i / (float)(num_platforms);

                // Create a host entity:
                auto entity = registry.create();

                // Attach an icon:
                auto& icon = registry.emplace<Icon>(entity);
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
                auto& transform = registry.emplace<Transform>(entity);
                transform.position = pos;

                // We need this to support the drop-line. There is a small performance hit.
                transform.topocentric = true;

                // Add a motion component to represent movement:
                double initial_bearing = -180.0 + rand_unit(mt) * 360.0;
                auto& motion = registry.emplace<MotionGreatCircle>(entity);
                motion.velocity = { -75000 + rand_unit(mt) * 150000, 0.0, 0.0 };
                motion.normalAxis = pos.srs.ellipsoid().greatCircleRotationAxis(glm::dvec3(lon, lat, 0.0), initial_bearing);

                // Add a labeling widget:
                auto& widget = registry.emplace<Widget>(entity);
                widget.text = std::to_string(i);
                widget.render = render_widget;

                // How about a drop line?
                // Since the drop line is relative to the platfrom, we have to enable
                // transform.localTangentPlane = true (see above)
                auto& drop_line = registry.emplace<Line>(entity);
                drop_line.points = { {0.0, 0.0, 0.0}, {0.0, 0.0, -1e6} };
                drop_line.style.width = 1.5f;
                drop_line.style.color = vsg::vec4{ 0.4f, 0.4f, 0.4f, 1.0f };

                // Decluttering control. The presence of this component will allow the entity
                // to participate in decluttering when it's enabled.
                auto& declutter = registry.emplace<Declutter>(entity);
                declutter.priority = alt;

                platforms.emplace(entity);
            }

            sim.run();
        }
    }

    ImGui::Text("Simulating %ld platforms", platforms.size());
    if (ImGuiLTable::Begin("sim"))
    {
        static bool show = true;
        if (ImGuiLTable::Checkbox("Show", &show))
        {
            auto [lock, registry] = app.registry.write();
            for (auto entity : platforms)
            {
                if (show)
                    registry.emplace_or_replace<ActiveState>(entity);
                else
                    registry.remove<ActiveState>(entity);
            }
        }

        ImGuiLTable::SliderFloat("Update rate", &sim.sim_hertz, 1.0f, 120.0f, "%.0f hz");

        ImGuiLTable::End();
    }
};
