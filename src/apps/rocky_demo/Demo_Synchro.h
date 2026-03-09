/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/ecs/MotionSystem.h>
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

using namespace std::chrono_literals;


auto Demo_Synchro = [](Application& app)
{
    static entt::entity e = entt::null;
    static double size[2] = { 1e5, 2e5 };
    static double longitude[2] = { 0.0, 2.0 };
    static int sizeIndex = 0;
    static std::shared_ptr<System> sys;
    static CallbackSubscription cb;
    static double step = 0.5;

    struct DataStoreSystem : public System
    {
        double _last = 0.0;
        DataStoreSystem(Registry& registry) : System(registry) {}

        static std::shared_ptr<DataStoreSystem> create(Registry& r) {
            return std::make_shared<DataStoreSystem>(r);
        }

        void update(VSGContext context) override
        {
            auto t = ((double)std::chrono::steady_clock::now().time_since_epoch().count()) / 1e9;
            auto d = std::fmod(t, step);
            if (d < _last)
            {
                _registry.write([&](entt::registry& reg)
                    {
                        auto& geom = reg.get<MeshGeometry>(e);
                        sizeIndex = 1 - sizeIndex;
                        geom.vertices = { {0,0,0}, {1 * size[sizeIndex],0,0}, {1 * size[sizeIndex],1 * size[sizeIndex],0}, {0,1 * size[sizeIndex],0} };
                        geom.dirty(reg);

                        auto& xform = reg.get<Transform>(e);
                        xform.position.x = longitude[sizeIndex];
                        xform.dirty();
                    });

                context->requestFrame();
            }
            _last = d;
        }
    };

    if (e == entt::null)
    {
        app.registry.write([&](entt::registry& reg)
            {
                e = reg.create();

                auto& geom = reg.emplace<MeshGeometry>(e);
                geom.vertices = { {0,0,0}, {1 * size[sizeIndex],0,0}, {1 * size[sizeIndex],1 * size[sizeIndex],0}, {0,1 * size[sizeIndex],0} };
                geom.indices = { 0, 1, 2, 0, 2, 3 };

                auto& style = reg.emplace<MeshStyle>(e);
                style.color = { 1, 0, 0, 1 };

                auto& mesh = reg.emplace<Mesh>(e, geom, style);

                auto& xform = reg.emplace<Transform>(e);
                xform.topocentric = true;
                xform.position = GeoPoint(SRS::WGS84, longitude[sizeIndex], 0, 1e6);
            });

        sys = DataStoreSystem::create(app.registry);
        sys->update(app.vsgcontext);

        app.systemsNode->add(sys);
    }

    ImGui::TextWrapped("%s", "Tests update synchronization between Transforms and ECS geometry");
    ImGui::TextWrapped("%s", "Pass: Small square on the left, large square on the right");

    if (ImGuiLTable::Begin("synchro demo"))
    {
        auto [_, reg] = app.registry.read();

        ImGuiLTable::SliderDouble("Step (s)", &step, 0.001, 2.0);
        ImGuiLTable::End();
    }
};
