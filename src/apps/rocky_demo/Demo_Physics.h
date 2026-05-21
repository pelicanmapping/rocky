/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

using namespace ROCKY_NAMESPACE;

namespace
{
    void PhysicsDemo_makeBox(MeshGeometry& geom, const glm::dvec3& halfExtents)
    {
        const auto& h = halfExtents;

        struct Face
        {
            glm::dvec3 normal;
            glm::dvec3 a;
            glm::dvec3 b;
            glm::dvec3 c;
            glm::dvec3 d;
        };

        const Face faces[] = {
            { { 0.0,  0.0, -1.0}, {-h.x, -h.y, -h.z}, {-h.x,  h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x, -h.y, -h.z} },
            { { 0.0,  0.0,  1.0}, {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z} },
            { {-1.0,  0.0,  0.0}, {-h.x, -h.y, -h.z}, {-h.x, -h.y,  h.z}, {-h.x,  h.y,  h.z}, {-h.x,  h.y, -h.z} },
            { { 1.0,  0.0,  0.0}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x,  h.y,  h.z}, { h.x, -h.y,  h.z} },
            { { 0.0, -1.0,  0.0}, {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x, -h.y,  h.z}, {-h.x, -h.y,  h.z} },
            { { 0.0,  1.0,  0.0}, {-h.x,  h.y, -h.z}, {-h.x,  h.y,  h.z}, { h.x,  h.y,  h.z}, { h.x,  h.y, -h.z} }
        };

        geom.vertices.reserve(24);
        geom.normals.reserve(24);
        geom.indices.reserve(36);

        for (auto& face : faces)
        {
            auto base = (std::uint32_t)geom.vertices.size();
            geom.vertices.insert(geom.vertices.end(), { face.a, face.b, face.c, face.d });
            for (int i = 0; i < 4; ++i)
                geom.normals.emplace_back(face.normal);
            geom.indices.insert(geom.indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
        }
    }

    void PhysicsDemo_makeSphere(MeshGeometry& geom, double radius, int rings = 14, int sectors = 28)
    {
        for (int r = 0; r <= rings; ++r)
        {
            double theta = glm::pi<double>() * (double)r / (double)rings;
            double st = std::sin(theta);
            double ct = std::cos(theta);
            for (int s = 0; s <= sectors; ++s)
            {
                double phi = glm::two_pi<double>() * (double)s / (double)sectors;
                glm::dvec3 n(std::cos(phi) * st, ct, std::sin(phi) * st);
                geom.vertices.emplace_back(n * radius);
                geom.normals.emplace_back(n);
            }
        }

        for (int r = 0; r < rings; ++r)
        {
            for (int s = 0; s < sectors; ++s)
            {
                auto i0 = (std::uint32_t)(r * (sectors + 1) + s);
                auto i1 = (std::uint32_t)(i0 + 1);
                auto i2 = (std::uint32_t)(i0 + sectors + 1);
                auto i3 = (std::uint32_t)(i2 + 1);
                geom.indices.insert(geom.indices.end(), { i0, i1, i2, i1, i3, i2 });
            }
        }
    }

    void PhysicsDemo_makeCapsule(MeshGeometry& geom, double radius, double halfHeight, int rings = 14, int sectors = 28)
    {
        int hemisphereRings = std::max(2, rings / 2);

        const auto addRing = [&](double theta, double centerY)
            {
                double st = std::sin(theta);
                double ct = std::cos(theta);

                for (int s = 0; s <= sectors; ++s)
                {
                    double phi = glm::two_pi<double>() * (double)s / (double)sectors;
                    glm::dvec3 n(std::cos(phi) * st, ct, std::sin(phi) * st);
                    geom.vertices.emplace_back(n.x * radius, centerY + n.y * radius, n.z * radius);
                    geom.normals.emplace_back(n);
                }
            };

        for (int r = 0; r <= hemisphereRings; ++r)
        {
            double theta = glm::half_pi<double>() * (double)r / (double)hemisphereRings;
            addRing(theta, halfHeight);
        }

        for (int r = 0; r <= hemisphereRings; ++r)
        {
            double theta = glm::half_pi<double>() + glm::half_pi<double>() * (double)r / (double)hemisphereRings;
            addRing(theta, -halfHeight);
        }

        int totalRings = hemisphereRings * 2 + 2;
        for (int r = 0; r < totalRings - 1; ++r)
        {
            for (int s = 0; s < sectors; ++s)
            {
                auto i0 = (std::uint32_t)(r * (sectors + 1) + s);
                auto i1 = (std::uint32_t)(i0 + 1);
                auto i2 = (std::uint32_t)(i0 + sectors + 1);
                auto i3 = (std::uint32_t)(i2 + 1);
                geom.indices.insert(geom.indices.end(), { i0, i1, i2, i1, i3, i2 });
            }
        }
    }

    Transform& PhysicsDemo_addTransform(entt::registry& registry, entt::entity entity, const GeoPoint& origin, const glm::dvec3& offset)
    {
        auto& transform = registry.emplace<Transform>(entity);
        transform.position = origin;
        transform.topocentric = true;
        transform.radius = 100.0;
        transform.localMatrix = glm::translate(glm::dmat4(1.0), offset);
        return transform;
    }

    entt::entity PhysicsDemo_addBox(
        entt::registry& registry,
        const GeoPoint& origin,
        const glm::dvec3& offset,
        const glm::dvec3& halfExtents,
        const Color& color,
        RigidBody::MotionType motion)
    {
        auto entity = registry.create();

        auto& geom = registry.emplace<MeshGeometry>(entity);
        PhysicsDemo_makeBox(geom, halfExtents);

        auto& style = registry.emplace<MeshStyle>(entity);
        style.color = color;
        style.lighting = true;

        registry.emplace<Mesh>(entity, geom, style);
        PhysicsDemo_addTransform(registry, entity, origin, offset);

        auto& body = registry.emplace<RigidBody>(entity);
        body.motion = motion;
        body.mass = (float)std::max(1.0, halfExtents.x * halfExtents.y * halfExtents.z * 8.0);
        body.material.friction = 0.65f;
        body.material.restitution = 0.15f;

        auto& box = registry.emplace<BoxCollider>(entity);
        box.halfExtents = halfExtents;

        return entity;
    }

    entt::entity PhysicsDemo_addSphere(
        entt::registry& registry,
        const GeoPoint& origin,
        const glm::dvec3& offset,
        double radius,
        const Color& color)
    {
        auto entity = registry.create();

        auto& geom = registry.emplace<MeshGeometry>(entity);
        PhysicsDemo_makeSphere(geom, radius);

        auto& style = registry.emplace<MeshStyle>(entity);
        style.color = color;
        style.lighting = true;

        registry.emplace<Mesh>(entity, geom, style);
        PhysicsDemo_addTransform(registry, entity, origin, offset);

        auto& body = registry.emplace<RigidBody>(entity);
        body.motion = RigidBody::MotionType::Dynamic;
        body.mass = (float)std::max(1.0, radius * radius * radius * 4.0);
        body.material.restitution = 0.45f;

        auto& sphere = registry.emplace<SphereCollider>(entity);
        sphere.radius = radius;

        return entity;
    }

    entt::entity PhysicsDemo_addCapsule(
        entt::registry& registry,
        const GeoPoint& origin,
        const glm::dvec3& offset,
        double radius,
        double halfHeight,
        const Color& color)
    {
        auto entity = registry.create();

        auto& geom = registry.emplace<MeshGeometry>(entity);
        PhysicsDemo_makeCapsule(geom, radius, halfHeight);

        auto& style = registry.emplace<MeshStyle>(entity);
        style.color = color;
        style.lighting = true;

        registry.emplace<Mesh>(entity, geom, style);
        auto& transform = PhysicsDemo_addTransform(registry, entity, origin, offset);
        transform.localMatrix = glm::translate(glm::dmat4(1.0), offset) *
            glm::mat4_cast(glm::angleAxis(glm::radians(25.0), glm::dvec3(0.0, 0.0, 1.0)));

        auto& body = registry.emplace<RigidBody>(entity);
        body.motion = RigidBody::MotionType::Dynamic;
        body.mass = 3.0f;
        body.material.restitution = 0.25f;

        auto& capsule = registry.emplace<CapsuleCollider>(entity);
        capsule.radius = radius;
        capsule.halfHeightOfCylinder = halfHeight;

        return entity;
    }
}

auto Demo_Physics = [](Application& app)
{
#ifdef ROCKY_HAS_JOLT
    static std::vector<entt::entity> entities;
    static entt::entity settingsEntity = entt::null;
    static bool initialized = false;
    static bool debugDraw = true;
    static bool terrainColliders = true;
    static bool terrainLeafColliders = true;
    static bool continuousWasEnabled = false;

    auto physics = app.systemsNode ? app.systemsNode->get<JoltPhysicsSystem>() : nullptr;
    if (!physics)
    {
        ImGui::TextUnformatted("Jolt physics system is not installed.");
        return;
    }

    if (!initialized)
    {
        continuousWasEnabled = app.renderContinuously;
        app.renderContinuously = true;

        const GeoPoint origin(SRS::WGS84, -105.2, 39.7, 2000.0);

        app.registry.write([&](entt::registry& registry)
            {
                settingsEntity = registry.create();
                auto& settings = registry.emplace<PhysicsSettings>(settingsEntity);
                settings.enabled = false;
                settings.debugDrawColliders = debugDraw;
                settings.terrainColliders = terrainColliders;
                settings.terrainCollidersUseLeafTiles = terrainLeafColliders;
                settings.islandRebaseDistance = 10000.0;

                entities.emplace_back(settingsEntity);

                entities.emplace_back(PhysicsDemo_addBox(
                    registry, origin, { 0.0, 0.0, 0.0 },
                    { 10.0, 10.0, 2.0 },
                    Color(0.25f, 0.25f, 0.28f, 1.0f),
                    RigidBody::MotionType::Static));

                const Color boxColors[] = {
                    Color(0.95f, 0.20f, 0.18f, 1.0f),
                    Color(0.10f, 0.65f, 1.00f, 1.0f),
                    Color(1.00f, 0.78f, 0.12f, 1.0f),
                    Color(0.20f, 0.90f, 0.35f, 1.0f),
                    Color(0.85f, 0.35f, 1.00f, 1.0f),
                    Color(0.00f, 0.85f, 0.78f, 1.0f)
                };

                for (int i = 0; i < 6; ++i)
                {
                    double x = -36.0 + i * 14.0;
                    double z = 20.0 + i * 13.0;
                    entities.emplace_back(PhysicsDemo_addBox(
                        registry, origin, { x, -12.0, z },
                        { 4.5, 4.5, 4.5 },
                        boxColors[i % (sizeof(boxColors) / sizeof(boxColors[0]))],
                        RigidBody::MotionType::Dynamic));
                }

                for (int i = 0; i < 5; ++i)
                {
                    double x = -28.0 + i * 14.0;
                    double z = 28.0 + i * 15.0;
                    entities.emplace_back(PhysicsDemo_addSphere(
                        registry, origin, { x, 16.0, z },
                        4.0,
                        Color(0.95f, 0.35f, 0.15f, 1.0f)));
                }

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { -24.0, 38.0, 50.0 },
                    3.5, 7.0,
                    Color(1.00f, 0.35f, 0.75f, 1.0f)));

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { 24.0, 38.0, 66.0 },
                    3.5, 7.0,
                    Color(0.35f, 1.00f, 0.95f, 1.0f)));
            });

        auto& view = app.display.window(0).view(0);
        if (auto manip = MapManipulator::get(view.vsgView))
        {
            Viewpoint vp;
            vp.point = origin;
            vp.range = 420.0;
            vp.pitch = -35.0;
            vp.heading = 30.0;
            manip->setViewpoint(vp, 1.0s);
        }

        initialized = true;
        app.vsgcontext->requestFrame();
    }

    if (ImGuiLTable::Begin("physics"))
    {
        app.registry.write([&](entt::registry& registry)
            {
                auto& settings = registry.get<PhysicsSettings>(settingsEntity);

                if (settings.enabled)
                {
                    if (ImGuiLTable::Button("Pause simulation"))
                    {
                        settings.enabled = false;
                        app.vsgcontext->requestFrame();
                    }
                }
                else
                {
                    if (ImGuiLTable::Button("Start simulation"))
                    {
                        settings.enabled = true;
                        app.vsgcontext->requestFrame();
                    }
                }

                if (ImGuiLTable::Checkbox("Debug colliders", &debugDraw))
                {
                    settings.debugDrawColliders = debugDraw;
                    app.vsgcontext->requestFrame();
                }

                if (ImGuiLTable::Checkbox("Terrain colliders", &terrainColliders))
                {
                    settings.terrainColliders = terrainColliders;
                    app.vsgcontext->requestFrame();
                }

                if (ImGuiLTable::Checkbox("Leaf terrain colliders", &terrainLeafColliders))
                {
                    settings.terrainCollidersUseLeafTiles = terrainLeafColliders;
                    app.vsgcontext->requestFrame();
                }

                float gravity = (float)settings.gravity.z;
                if (ImGuiLTable::SliderFloat("Gravity", &gravity, -30.0f, 0.0f, "%.1f m/s^2"))
                {
                    settings.gravity.z = gravity;
                    app.vsgcontext->requestFrame();
                }

                float stepHz = (float)(1.0 / settings.fixedTimeStep);
                if (ImGuiLTable::SliderFloat("Step rate", &stepHz, 30.0f, 240.0f, "%.0f hz"))
                {
                    settings.fixedTimeStep = 1.0 / std::max(1.0f, stepHz);
                    app.vsgcontext->requestFrame();
                }

                int maxSubSteps = settings.maxSubSteps;
                if (ImGuiLTable::SliderInt("Max substeps", &maxSubSteps, 1, 16))
                {
                    settings.maxSubSteps = std::max(1, maxSubSteps);
                    app.vsgcontext->requestFrame();
                }

                ImGuiLTable::Text("Bodies", "%zu", physics->numBodies());
            });

        if (ImGuiLTable::Button("Reset scene"))
        {
            app.registry.write([&](entt::registry& registry)
                {
                    for (auto entity : entities)
                        if (registry.valid(entity))
                            registry.destroy(entity);
                    entities.clear();
                    settingsEntity = entt::null;
                    initialized = false;
                });
            app.vsgcontext->requestFrame();
        }

        if (ImGuiLTable::Button("Stop demo"))
        {
            app.registry.write([&](entt::registry& registry)
                {
                    for (auto entity : entities)
                        if (registry.valid(entity))
                            registry.destroy(entity);
                    entities.clear();
                    settingsEntity = entt::null;
                });
            app.renderContinuously = continuousWasEnabled;
            initialized = false;
            app.vsgcontext->requestFrame();
        }

        ImGuiLTable::End();
    }
#else
    ImGui::TextUnformatted("Rocky was built without Jolt Physics support.");
#endif
};
