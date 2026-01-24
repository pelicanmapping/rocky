/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Motion.h>
#include <rocky/ecs/Registry.h>
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/ecs/System.h>

namespace ROCKY_NAMESPACE
{
    /**
    * ECS System to process Motion components
    */
    class MotionSystem : public System
    {
    public:
        MotionSystem(Registry& r) : System(r) { }

        static std::shared_ptr<MotionSystem> create(Registry& r) {
            return std::make_shared<MotionSystem>(r); }

        //! Called periodically to update the transforms
        void update(VSGContext& context) override
        {
            auto time = context->viewer()->getFrameStamp()->time;

            if (last_time != vsg::time_point::min())
            {
                auto [lock, reg] = _registry.read();

                const glm::dvec3 zero{ 0.0, 0.0, 0.0 };

                // delta seconds since last tick:
                double dt = 1e-9 * (double)(time - last_time).count();

                // Join query all motions + transform pairs:
                reg.view<Motion, Transform, TransformDetail>().each([&](auto& motion, auto& transform, auto& transform_detail)
                    {
                        if (motion.velocity != zero && transform.revision == transform_detail.sync.revision)
                        {
                            auto& pos = transform.position;

                            SRSOperation pos_to_world;
                            if (!pos.srs.isGeocentric())
                                pos_to_world = pos.srs.to(pos.srs.geocentricSRS());

                            // move the entity using a velocity vector in the local tangent plane
                            glm::dvec3 world;
                            pos_to_world(pos, world);
                            auto l2w = pos.srs.ellipsoid().topocentricToGeocentricMatrix(world);

                            world = l2w * (motion.velocity * dt);

                            pos_to_world.inverse(world, pos);

                            transform.dirty(reg);
                        }

                        motion.velocity += motion.acceleration * dt;
                    });

                reg.view<MotionGreatCircle, Transform, TransformDetail>().each([&](auto& motion, auto& transform, auto& detail)
                    {
                        // Note. For this demo, we just use the length of the velocity and acceleration
                        // vectors and ignore direction.
                        if (motion.velocity != zero && transform.revision == detail.sync.revision)
                        {
                            auto& pos = transform.position;

                            SRSOperation pos_to_world;
                            if (!pos.srs.isGeocentric())
                                pos_to_world = pos.srs.to(pos.srs.geocentricSRS());

                            glm::dvec3 world;
                            pos_to_world(pos, world);

                            // calculate the rotation angle for the distance to travel:
                            double distance = glm::length(motion.velocity * dt);
                            double R = glm::length(world);
                            double circ = 2.0 * glm::pi<double>() * R;
                            double angle = 360.0 * distance / circ;

                            // bailout if the time delta was too small to cause any motion
                            if (glm::epsilonEqual(distance, 0.0, 1e-6) || glm::epsilonEqual(angle, 0.0, 1e-6))
                                return;

                            // move the point:
                            pos = pos.srs.ellipsoid().rotate(world, motion.normalAxis, angle);

                            transform.dirty(reg);
                        }

                        motion.velocity += motion.acceleration * dt;
                    });
            }

            last_time = time;
        }

    private:
        vsg::time_point last_time = vsg::time_point::min();
    };
}
