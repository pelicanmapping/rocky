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

    /**
    * ECS System to process Motion components
    */
    class MotionSystem : public ECS::System
    {
    public:
        MotionSystem(entt::registry& r) : ECS::System(r) { }

        std::vector<entt::entity> updated;

        //! Called periodically to update the transforms
        void update(Runtime& runtime) override
        {
            updated.clear();

            auto time = runtime.viewer->getFrameStamp()->time;

            if (last_time != ECS::time_point::min())
            {
                // delta seconds since last tick:
                double dt = 1e-9 * (double)(time - last_time).count();

                // Join query all motions + transform pairs:
                auto view = registry.group<Motion, Transform>();

                for (auto&& [entity, motion, transform] : view.each())
                {
                    const glm::dvec3 zero{ 0.0, 0.0, 0.0 };

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

                        updated.emplace_back(entity);
                    }

                    motion.velocity += motion.acceleration * dt;
                };
            }
            last_time = time;
        }

    private:
        //! Constructor
        ECS::time_point last_time = ECS::time_point::min();
    };
}
