/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "ECS.h"
#include "json.h"
#include <vsg/vk/State.h>

ROCKY_ABOUT(entt, ENTT_VERSION);

using namespace ROCKY_NAMESPACE;


JSON
ECS::Component::to_json() const
{
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}

void
ECS::SystemsManager::update(ECS::time_point time)
{
    for (auto& system : systems)
    {
        system->update(time);
    }
}

void
EntityMotionSystem::update(ECS::time_point time)
{
    if (last_time != ECS::time_point::min())
    {
        // delta seconds since last tick:
        double dt = 1e-9 * (double)(time - last_time).count();

        // Join query all motions + transform pairs:
        auto view = registry.group<Motion, Transform>();

        view.each([dt](const auto entity, auto& motion, auto& transform)
            {
                // move the entity using a velocity vector in the local tangent plane
                auto& pos = transform.node->position;
                auto l2w = pos.srs.localToWorldMatrix({ pos.x, pos.y, pos.z });
                auto world = l2w * (motion.velocity * dt);
                motion.velocity += motion.acceleration * dt;

                if (!motion.world2pos.valid())
                    motion.world2pos = pos.srs.geocentricSRS().to(pos.srs);

                vsg::dvec3 coord(world.x, world.y, world.z);
                motion.world2pos(coord, coord);

                pos.x = coord.x, pos.y = coord.y, pos.z = coord.z;
                transform.node->dirty();
            });
    }
    last_time = time;
}
