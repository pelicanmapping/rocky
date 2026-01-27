/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs/MotionSystem.h>
#include <rocky/vsg/imgui/ImGuiImage.h>
#include <random>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;

namespace
{
    // Custom component that represents a simulated object
    struct SimulatedPlatform
    {
        std::string name;
    };

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

    // Create a number of entities with random positions and motions
    std::vector<entt::entity> createEntities(entt::registry& registry, unsigned count, 
        ImGuiImage& image, bool& showPosition, VSGContext context)
    {
        std::vector<entt::entity> entities;

        auto renderEntity = [&image, &showPosition, context](WidgetInstance& i)
            {
                auto& platform = i.registry.get<SimulatedPlatform>(i.entity);
                auto& xform = i.registry.get<Transform>(i.entity);

                auto pointECEF = xform.position.transform(SRS::ECEF);

                float red = ((int)i.entity % 3) == 0 ? 0.5f : 0.0f;
                float green = ((int)i.entity % 3) == 1 ? 0.5f : 0.0f;
                float blue = ((int)i.entity % 3) == 2 ? 0.5f : 0.0f;

                ImGui::SetCurrentContext(i.context);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(red, green, blue, 0.65f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.0f));

                ImGui::SetNextWindowPos(
                    ImVec2{ i.position.x - image.size().x/2, i.position.y - image.size().y / 2 },
                    ImGuiCond_Always,
                    ImVec2{ 0.0f, 0.0f }); // pivot point in the upper left

                ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);
                {
                    // calculate the bearing for our icon:
                    auto& motion = i.registry.get<MotionGreatCircle>(i.entity);
                    auto heading = pointECEF.srs.ellipsoid().heading(pointECEF, motion.normalAxis);

                    if (ImGui::BeginTable("asset", 2))
                    {
                        ImGui::TableNextColumn();
                        image.render(image.size(), heading);

                        ImGui::TableNextColumn();
                        ImGui::Text("%s", platform.name.c_str());

                        if (showPosition)
                        {
                            auto pointWGS84 = pointECEF.transform(SRS::WGS84);
                            ImGui::Separator();
                            ImGui::Text("Pos: %.2f, %.2f", pointWGS84.y, pointWGS84.x);
                            ImGui::Separator();
                            ImGui::Text("Alt: %.0f m", pointWGS84.z);
                            ImGui::Separator();
                            ImGui::Text("Hdg: %.1f", heading);
                        }

                        ImGui::EndTable();
                    }
                }
                auto size = ImGui::GetWindowSize();
                ImGui::End();

                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);

                // update decluttering volume
                auto& dc = i.registry.get<Declutter>(i.entity);
                dc.rect = Rect(0, 0, size.x, size.y);
            };

        auto ll_to_ecef = SRS::WGS84.to(SRS::ECEF);

        // Make the drop-line style and geometry. We can share this across all simulated platforms.
        auto dropEntity = registry.create();

        auto& dropStyle = registry.emplace<LineStyle>(dropEntity);
        dropStyle.width = 2.0f;
        dropStyle.color = Color{ 0.8f, 0.4f, 0.4f, 1.0f };

        auto& dropGeom = registry.emplace<LineGeometry>(dropEntity);
        dropGeom.points = { {0.0, 0.0, 0.0}, {0.0, 0.0, -1e6} };
        

        entities.reserve(count);

        std::mt19937 mt;
        std::uniform_real_distribution<float> rand_unit(0.0, 1.0);

        for (unsigned i = 0; i < count; ++i)
        {
            float t = (float)i / (float)(count);

            // Create a host entity & platform:
            auto entity = registry.create();

            auto& platform = registry.emplace<SimulatedPlatform>(entity);
            platform.name = std::string("Sim " + std::to_string(i + 1));

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

            // Add a widget to render the HUD:
            auto& widget = registry.emplace<Widget>(entity);
            widget.render = renderEntity;

            // Add the dropline for this entity:
            registry.emplace<Line>(entity, dropGeom, dropStyle);

            // Decluttering control. The presence of this component will allow the entity
            // to participate in decluttering when it's enabled.
            auto& declutter = registry.emplace<Declutter>(entity);
            declutter.priority = alt;

            entities.emplace_back(entity);
        }

        return entities;
    }
}

auto Demo_Simulation = [](Application& app)
{
    const char* icon_location = "https://readymap.org/readymap/filemanager/download/public/icons/airport.png";

    // Make an entity for us to tether to and set it in motion
    static NodeLayer::Ptr layer;
    static vsg::ref_ptr<EntityNode> entityNode;
    static Status status;
    static Simulator sim(app);
    static ImGuiImage widgetImage;
    static unsigned num_platforms = 1500;
    static bool showPosition = false;
    static bool tethering = false;

    if (status.failed())
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", "Image load failed!");
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", status.error().message.c_str());
        return;
    }

    const float s = 20.0;

    // Load an image to use in our widget.
    if (!widgetImage)
    {
        // add an icon:
        auto io = app.vsgcontext->io;
        auto image = io.services().readImageFromURI(icon_location, io);
        if (image.ok())
        {
            image.value()->flipVerticalInPlace();
            widgetImage = ImGuiImage(image.value(), app.vsgcontext);
        }
    }

    // Create the layer, the entity node, and start up the background sim thread.
    if (!layer)
    {
        entityNode = EntityNode::create(app.registry);

        layer = NodeLayer::create(entityNode);
        layer->name = "Simulation Entities";
        if (layer->open(app.io()))
            app.mapNode->map->add(layer);

        sim.run();

        app.vsgcontext->requestFrame();
    }

    // Create some entities!
    if (entityNode->entities.empty())
    {
        app.registry.write([&](entt::registry& registry)
            {
                entityNode->entities = createEntities(registry, num_platforms,
                    widgetImage, showPosition, app.vsgcontext);
            });
    }

    ImGui::Text("%s", "TIP: toggle visibility in the Map panel!");
    ImGui::Text("%s", "TIP: prevent overlap in the Decluttering panel!");
    ImGui::Separator();

    if (ImGuiLTable::Begin("simulation"))
    {
        ImGuiLTable::SliderInt("Entities", (int*)&num_platforms, 1, 5000);
        if (ImGuiLTable::Button("Refresh"))
        {
            if (tethering)
            {
                auto view = app.display.views(app.display.mainWindow()).front();
                auto manip = MapManipulator::get(view);
                manip->home();
            }

            app.registry.write([&](entt::registry& r)
                {
                    r.destroy(entityNode->entities.begin(), entityNode->entities.end());
                });

            entityNode->entities.clear();
        }

        ImGuiLTable::SliderFloat("Update rate", &sim.sim_hertz, 1.0f, 120.0f, "%.0f hz");

        ImGuiLTable::Checkbox("Show position", &showPosition);

        if (ImGuiLTable::Checkbox("Tethering", &tethering))
        {
            auto view = app.display.views(app.display.mainWindow()).front();
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
                        return app.registry.read()->get<Transform>(*entityNode->entities.begin()).position;
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
