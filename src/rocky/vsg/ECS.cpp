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

#if 0
SRSOperation
ECS::NodeComponent::setReferencePoint(const GeoPoint& point)
{
    SRS worldSRS = point.srs;

    if (point.srs.valid())
    {
        if (point.srs.isGeodetic())
        {
            worldSRS = point.srs.geocentricSRS();
            GeoPoint world = point.transform(worldSRS);
            if (world.valid())
            {
                refPoint = vsg::dvec3{ world.x, world.y, world.z };
            }
        }
        else
        {
            refPoint = vsg::dvec3{ point.x, point.y, point.z };
        }
    }

    return SRSOperation(point.srs, worldSRS);
}
#endif

void
EntityMotionSystem::update(Runtime& runtime)
{
    auto time = runtime.viewer->getFrameStamp()->time;

    if (last_time != ECS::time_point::min())
    {
        // delta seconds since last tick:
        double dt = 1e-9 * (double)(time - last_time).count();

        // Join query all motions + transform pairs:
        auto view = registry.group<Motion, Transform>();

        view.each([dt](const auto entity, auto& motion, auto& transform)
            {
                const glm::dvec3 zero{ 0.0, 0.0, 0.0 };

                if (motion.velocity != zero)
                {
                    auto& pos = transform.node->position;
                    double save_z = pos.z;

                    auto pos_to_world = pos.srs.to(pos.srs.geocentricSRS());

                    // move the entity using a velocity vector in the local tangent plane
                    glm::dvec3 world;
                    pos_to_world((glm::dvec3)pos, world);
                    auto l2w = pos.srs.ellipsoid().geocentricToLocalToWorld(world);

                    world = l2w * (motion.velocity * dt);

                    vsg::dvec3 coord(world.x, world.y, world.z);
                    pos_to_world.inverse(coord, coord);

                    pos.x = coord.x, pos.y = coord.y;
                    pos.z = save_z;
                    transform.node->dirty();
                }

                motion.velocity += motion.acceleration * dt;
            });
    }
    last_time = time;
}
