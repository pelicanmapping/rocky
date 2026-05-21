/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/ecs/Component.h>
#include <rocky/SRS.h>
#include <glm/gtc/quaternion.hpp>

namespace ROCKY_NAMESPACE
{
    //! Global physics simulation settings.
    struct PhysicsSettings : public Component<PhysicsSettings>
    {
        bool enabled = true;
        bool debugDrawColliders = false;
        bool terrainColliders = true;

        //! Only simulate terrain colliders for resident tiles without resident children.
        bool terrainCollidersUseLeafTiles = true;

        //! Gravity in the local physics island frame, meters/second^2.
        glm::dvec3 gravity = { 0.0, 0.0, -9.80665 };

        //! Fixed simulation step, seconds.
        double fixedTimeStep = 1.0 / 60.0;

        //! Maximum fixed steps to execute in one rendered frame.
        int maxSubSteps = 8;

        //! Jolt world sizing defaults. These apply when the backend is initialized.
        std::uint32_t maxBodies = 65536;
        std::uint32_t numBodyMutexes = 0;
        std::uint32_t maxBodyPairs = 65536;
        std::uint32_t maxContactConstraints = 20480;
        std::uint32_t tempAllocatorSizeMB = 32;

        //! Recenter the hidden local physics island when bodies drift this far.
        double islandRebaseDistance = 20000.0;
    };

    //! Common rigid body material parameters.
    struct PhysicsMaterial
    {
        float friction = 0.5f;
        float restitution = 0.0f;
    };

    //! Rigid body component. Pair this with one or more Collider components.
    struct RigidBody : public Component<RigidBody>
    {
        enum class MotionType
        {
            Static,
            Kinematic,
            Dynamic
        };

        MotionType motion = MotionType::Dynamic;
        PhysicsMaterial material;
        float mass = 1.0f;
        float linearDamping = 0.05f;
        float angularDamping = 0.05f;
        float gravityFactor = 1.0f;
        bool allowSleeping = true;
        bool sensor = false;

        glm::dvec3 linearVelocity = { 0.0, 0.0, 0.0 };
        glm::dvec3 angularVelocity = { 0.0, 0.0, 0.0 };
    };

    //! Local collider pose relative to the RigidBody Transform.
    struct ColliderPose
    {
        glm::dvec3 offset = { 0.0, 0.0, 0.0 };
        glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
        bool enabled = true;
    };

    //! Box collider, centered on the body origin by default.
    struct BoxCollider : public Component<BoxCollider>
    {
        ColliderPose pose;
        glm::dvec3 halfExtents = { 0.5, 0.5, 0.5 };
        float convexRadius = 0.02f;
    };

    //! Sphere collider.
    struct SphereCollider : public Component<SphereCollider>
    {
        ColliderPose pose;
        double radius = 0.5;
    };

    //! Capsule collider. The capsule's local axis is +Y, matching Jolt.
    struct CapsuleCollider : public Component<CapsuleCollider>
    {
        ColliderPose pose;
        double radius = 0.5;
        double halfHeightOfCylinder = 0.5;
    };

    //! Cylinder collider. The cylinder's local axis is +Y, matching Jolt.
    struct CylinderCollider : public Component<CylinderCollider>
    {
        ColliderPose pose;
        double radius = 0.5;
        double halfHeight = 0.5;
        float convexRadius = 0.02f;
    };

    //! Triangle mesh collider. Mesh colliders are best for static bodies.
    struct MeshCollider : public Component<MeshCollider>
    {
        ColliderPose pose;
        std::vector<glm::dvec3> vertices;
        std::vector<std::uint32_t> indices;
    };

    //! Convex hull collider. Dynamic mesh-like bodies should usually use this.
    struct ConvexHullCollider : public Component<ConvexHullCollider>
    {
        ColliderPose pose;
        std::vector<glm::dvec3> points;
        float convexRadius = 0.02f;
    };
}
