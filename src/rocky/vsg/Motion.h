/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ECS.h>
#include <rocky/vsg/Transform.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS Component applying simple motion to an object
    */
    struct Motion
    {
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
    };

    struct MotionGreatCircle : public Motion
    {
        glm::dvec3 normalAxis;
    };

    /**
    * ECS System to process Motion components
    */
    class MotionSystem : public ecs::System
    {
    public:
        MotionSystem(entt::registry& r) : ecs::System(r) { }

        //! Called periodically to update the transforms
        void update(Runtime& runtime) override
        {
            auto time = runtime.viewer->getFrameStamp()->time;

            if (last_time != ecs::time_point::min())
            {
                const glm::dvec3 zero{ 0.0, 0.0, 0.0 };

                // delta seconds since last tick:
                double dt = 1e-9 * (double)(time - last_time).count();

                // Join query all motions + transform pairs:
                for (auto&& [entity, motion, transform] : registry.view<Motion, Transform>().each())
                {

                    if (motion.velocity != zero)
                    {
                        GeoPoint& pos = transform.position;
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
                        transform.dirty();
                    }

                    motion.velocity += motion.acceleration * dt;
                }

                for (auto&& [entity, motion, transform] : registry.view<MotionGreatCircle, Transform>().each())
                {
                    if (motion.velocity != zero)
                    {
                        GeoPoint& pos = transform.position;
                        double save_z = pos.z;
                        auto pos_to_world = pos.srs.to(pos.srs.geocentricSRS());

                        glm::dvec3 world;
                        pos_to_world((glm::dvec3)pos, world);
                        double distance = glm::length(motion.velocity * dt);
                        double R = glm::length(world);
                        double circ = 2.0 * glm::pi<double>() * R;
                        double angle = 360.0 * distance / circ;

                        glm::dquat rot = glm::angleAxis(deg2rad(angle), motion.normalAxis);
                        auto temp = rot * world;
                        pos_to_world.inverse(temp, temp);
                        pos.x = temp.x, pos.y = temp.y, pos.z = temp.z;

                        transform.dirty();
                    }
                }
            }
            last_time = time;
        }

    private:
        //! Constructor
        ecs::time_point last_time = ecs::time_point::min();
    };
}
