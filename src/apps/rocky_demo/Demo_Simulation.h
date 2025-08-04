/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs/MotionSystem.h>
#include <rocky/vsg/imgui/ImGuiImage.h>
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
    static EntityCollectionLayer::Ptr layer;
    //static std::set<entt::entity> platforms;
    static Status status;
    static Simulator sim(app);
    static ImGuiImage widgetImage;
    const unsigned num_platforms = 1000;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Image load failed!");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), status.error().message.c_str());
        return;
    }

    const float s = 20.0;

    if (!widgetImage)
    {
        // add an icon:
        auto io = app.vsgcontext->io;
        auto image = io.services.readImageFromURI(icon_location, io);
        if (image.ok())
        {
            image.value()->flipVerticalInPlace();
            widgetImage = ImGuiImage(image.value(), app.vsgcontext);
        }
    }

    if (!layer)
    {
        layer = EntityCollectionLayer::create(app.registry);
        layer->name = "Simulation Entities";
        if (layer->open(app.io()))
            app.mapNode->map->add(layer);

        auto [lock, registry] = app.registry.write();

        std::mt19937 mt;
        std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

        auto render_widget = [&](WidgetInstance& i)
            {
                Transform& t = i.registry.get<Transform>(i.entity);

                auto point = t.position.transform(SRS::WGS84);

                i.begin();

                i.center.x += (i.size.x / 2) - (widgetImage.size().y / 2);
                i.center.y += (i.size.y / 2) - (widgetImage.size().y / 2);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.0f));

                i.render([&]()
                    {
                        auto deviceID = app.vsgcontext->device()->deviceID;
                        if (ImGui::BeginTable("asset", 2))
                        {
                            ImGui::TableNextColumn();
                            ImGui::Image(widgetImage.id(deviceID), widgetImage.size());

                            ImGui::TableNextColumn();
                            ImGui::Text("ID:  %s", i.widget.text.c_str());
                            ImGui::Separator();
                            ImGui::Text("Lat: %.2f", point.y);
                            ImGui::Separator();
                            ImGui::Text("Lon: %.2f", point.x);
                            ImGui::Separator();
                            ImGui::Text("Alt: %.0f", point.z);

                            ImGui::EndTable();
                        }
                    });

                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();

                i.end();

                // update decluttering volume
                auto& dc = i.registry.get<Declutter>(i.entity);
                dc.rect = Rect(i.size.x, i.size.y);
            };

        auto ll_to_ecef = SRS::WGS84.to(SRS::ECEF);

        layer->entities.reserve(num_platforms);

        for (unsigned i = 0; i < num_platforms; ++i)
        {
            float t = (float)i / (float)(num_platforms);

            // Create a host entity:
            auto entity = registry.create();

            double lat = -80.0 + rand_unit(mt) * 160.0;
            double lon = -180 + rand_unit(mt) * 360.0;
            double alt = 1000.0 + t * 100000.0;
            GeoPoint pos_ecef = GeoPoint(SRS::WGS84, lon, lat, alt).transform(SRS::ECEF);

            // Add a transform component:
            auto& transform = registry.emplace<Transform>(entity);
            transform.position = pos_ecef;

            // We need this to support the drop-line. There is a small performance hit.
            transform.topocentric = true;

            // Add a motion component to represent movement:
            double initial_bearing = -180.0 + rand_unit(mt) * 360.0;
            auto& motion = registry.emplace<MotionGreatCircle>(entity);
            motion.velocity = { -7500 + rand_unit(mt) * 15000, 0.0, 0.0 };
            motion.normalAxis = pos_ecef.srs.ellipsoid().rotationAxis(pos_ecef, initial_bearing);

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
            drop_line.style.color = Color{ 0.4f, 0.4f, 0.4f, 1.0f };

            // Decluttering control. The presence of this component will allow the entity
            // to participate in decluttering when it's enabled.
            auto& declutter = registry.emplace<Declutter>(entity);
            declutter.priority = alt;

            layer->entities.emplace_back(entity);
            //platforms.emplace(entity);
        }

        sim.run();

        app.vsgcontext->requestFrame();
    }

    ImGui::Text("Simulating %ld platforms", layer->entities.size());
    ImGui::Text("NOTE: toggle visibility in the Map panel");

    if (ImGuiLTable::Begin("simulation"))
    {
        ImGuiLTable::SliderFloat("Update rate", &sim.sim_hertz, 1.0f, 120.0f, "%.0f hz");

        static bool tethering = false;
        if (ImGuiLTable::Checkbox("Tethering", &tethering))
        {
            auto main_window = app.display.windowsAndViews.begin();
            auto view = main_window->second.front();
            auto manip = MapManipulator::get(view);
            
            if (tethering)
            {
                // To tether, create a Viewpoint object with a "pointFunction" that
                // returns the current location of the tracked object.
                Viewpoint vp;
                vp.range = 1e6;
                vp.pitch = -45;
                vp.heading = 45;
                vp.pointFunction = [&]()
                    {
                        auto [lock, registry] = app.registry.read();
                        return registry.get<Transform>(*layer->entities.begin()).position;
                    };

                manip->setViewpoint(vp, 2.0s);
            }
            else
            {
                manip->home();
            }
        }

        ImGuiLTable::End();
    }
};
