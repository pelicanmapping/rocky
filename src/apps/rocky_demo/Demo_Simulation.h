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
    struct Declutter
    {
        bool dummy = false;
    };

    class DeclutterSystem : public ECS::System
    {
    public:
        DeclutterSystem(entt::registry& registry) : System(registry) { }

        double buffer_radius = 25.0;
        unsigned total = 0, visible = 1;
        std::size_t last_max_size = 32;
        std::function<std::vector<std::uint32_t>()> getActiveViewIDs;

        static std::shared_ptr<DeclutterSystem> create(entt::registry& registry) {
            return std::make_shared<DeclutterSystem>(registry);
        }

        void initializeSystem(Runtime& runtime) override
        {
            //nop
        }

        void update(Runtime& runtime) override
        {
            total = 0, visible = 0;

            auto viewIDs = getActiveViewIDs(); // copy

            for(auto viewID = 0; viewID < viewIDs.size(); ++viewID)
            {
                if (viewIDs[viewID] == 0) // skip unused
                    continue;

                // First collect all declutter-able entities and sort them by their distance to the camera.
                std::vector<std::tuple<entt::entity, double, double, double>> sorted; // entity, x, y, depth, radius
                sorted.reserve(last_max_size);

                double aspect_ratio = 1.0; // same for all objects
                auto view = registry.view<Declutter, Transform>();
                for (auto&& [entity, declutter, transform] : view.each())
                {
                    if (transform.node && transform.node->viewLocal.size() > viewID)
                    {
                        // Cheat by directly accessing view 0. In reality we will might either declutter per-view
                        // or have a "driving view" that controls visibility for all views.
                        auto& viewLocal = transform.node->viewLocal[viewID];
                        auto& mvp = viewLocal.mvp;    
                        auto clip = mvp[3] / mvp[3][3];
                        vsg::dvec2 window((clip.x + 1.0) * 0.5 * (double)viewLocal.viewport[2], (clip.y + 1.0) * 0.5 * (double)viewLocal.viewport[3]);
                        sorted.emplace_back(entity, window.x, window.y, clip.z);
                    }
                }
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return std::get<3>(a) > std::get<3>(b); }); // reverse z buffer
                last_max_size = sorted.size();

                // Next, take the sorted vector and declutter by populating an R-Tree with rectangles representing
                // each entity's buffered location in screen(clip) space. For objects that don't conflict with
                // higher-priority objects, set visibility to true.
                RTree<entt::entity, double, 2> rtree;
                double bh = 0.5 * buffer_radius;

                for (auto iter : sorted)
                {
                    ++total;

                    auto [entity, x, y, z] = iter;

                    auto& visibility = registry.get<Visibility>(entity);
                    double LL[2]{ x - bh, y - bh * aspect_ratio };
                    double UR[2]{ x + bh, y + bh * aspect_ratio };

                    if (rtree.Search(LL, UR, [](auto e) { return false; }) == 0)
                    {
                        rtree.Insert(LL, UR, entity);
                        visibility[viewID] = true;
                        ++visible;
                    }
                    else
                    {
                        visibility[viewID] = false;
                    }
                }
            }
        }

        void resetVisibility()
        {
            auto view = registry.view<Declutter, Visibility>();
            for (auto&& [entity, declutter, visibility] : view.each())
            {
                visibility.setAll(true);
            }
        }
    };

    // Simple simulation system runinng in its own thread.
    // It uses a MotionSystem to process Motion components and update
    // their corresponding Transform components.
    class Simulator
    {
    public:
        Application& app;
        MotionSystem motion;
        DeclutterSystem declutter;
        float sim_hertz = 5.0f; // updates per second
        float declutter_hertz = 1.0f; // updates per second
        bool declutterEnabled = false;

        jobs::detail::event declutter_job_active;

        Simulator(Application& in_app) :
            app(in_app),
            motion(in_app.entities),
            declutter(in_app.entities) { }

        void run()
        {
            jobs::context context;
            context.pool = jobs::get_pool("rocky.simulation", 2);

            jobs::dispatch([this]()
                {
                    scoped_use use(app.handle);

                    while (app.active())
                    {
                        run_at_frequency f(sim_hertz);
                        motion.update(app.runtime());
                        app.runtime().requestFrame();

                        if (declutterEnabled && !declutter_job_active)
                            declutter_job_active = true;
                    }
                    declutter_job_active = true; // wake up the declutter thread so it can exit
                    Log()->info("Simulation thread terminating.");

                }, context);

            jobs::dispatch([this]()
                {
                    scoped_use use(app.handle);

                    while (declutter_job_active.wait() && app.active())
                    {
                        if (declutterEnabled)
                        {
                            run_at_frequency f(declutter_hertz);
                            declutter.update(app.runtime());
                            app.runtime().requestFrame();
                        }
                        else
                        {
                            declutter.resetVisibility();
                            declutter_job_active = false;
                        }
                    }
                    Log()->info("Declutter thread terminating.");

                }, context);
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
    const unsigned num_platforms = 100;

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

                // Label the platform:
                auto& label = app.entities.emplace<Label>(entity);
                label.text = std::to_string(i);
                label.style.font = app.runtime().defaultFont;
                label.style.pointSize = 16.0f;
                label.style.outlineSize = 0.2f;

                // Activate decluttering.
                app.entities.emplace<Declutter>(entity);

                platforms.emplace(entity);
            }

            // tell the declutterer how to access view IDs.
            sim.declutter.getActiveViewIDs = [&app]() { return app.displayManager->activeViewIDs; };

            Log()->info("Starting simulation threads.");
            sim.run();
        }
    }

    ImGui::Text("Simulating %ld platforms", platforms.size());
    if (ImGuiLTable::Begin("sim"))
    {
        ImGuiLTable::SliderFloat("Update rate", &sim.sim_hertz, 1.0f, 120.0f, "%.0f hz");

        if (ImGuiLTable::Checkbox("Decluttering", &sim.declutterEnabled)) {
            if (!sim.declutterEnabled)
                sim.declutter.resetVisibility();
        }

        if (sim.declutterEnabled)
        {
            ImGuiLTable::SliderDouble("  Radius", &sim.declutter.buffer_radius, 0.0f, 50.0f, "%.0f px");
            ImGuiLTable::SliderFloat("  Frequency", &sim.declutter_hertz, 1.0f, 30.0f, "%.0f hz");
            ImGuiLTable::Text("  Candidates", "%ld / %ld", sim.declutter.visible, sim.declutter.total);
        }
        ImGuiLTable::End();
    }
};
