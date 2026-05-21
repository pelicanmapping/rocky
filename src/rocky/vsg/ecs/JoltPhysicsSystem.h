/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/System.h>
#include <rocky/ecs/Physics.h>

namespace ROCKY_NAMESPACE
{
    class TerrainNode;

    /**
    * Jolt-backed rigid body physics system.
    *
    * The public ECS components live in rocky/ecs/Physics.h; this class is only
    * the backend that translates those components into a Jolt simulation.
    */
    class ROCKY_EXPORT JoltPhysicsSystem : public System
    {
    public:
        JoltPhysicsSystem(Registry& registry, vsg::ref_ptr<TerrainNode> terrain = { });
        ~JoltPhysicsSystem();

        static std::shared_ptr<JoltPhysicsSystem> create(Registry& registry, vsg::ref_ptr<TerrainNode> terrain = { })
        {
            return std::make_shared<JoltPhysicsSystem>(registry, terrain);
        }

        void initialize(VSGContext context) override;
        void update(VSGContext context) override;

        //! Sets the terrain source for paged mesh colliders.
        void setTerrainNode(vsg::ref_ptr<TerrainNode> terrain);

        //! Number of live Jolt bodies currently tracked by the system.
        std::size_t numBodies() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
