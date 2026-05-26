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

    void PhysicsDemo_makeCylinder(MeshGeometry& geom, double radius, double halfHeight, int sectors = 28)
    {
        const std::uint32_t sideStart = (std::uint32_t)geom.vertices.size();
        for (int ring = 0; ring < 2; ++ring)
        {
            double y = ring == 0 ? -halfHeight : halfHeight;
            for (int s = 0; s <= sectors; ++s)
            {
                double phi = glm::two_pi<double>() * (double)s / (double)sectors;
                glm::dvec3 n(std::cos(phi), 0.0, std::sin(phi));
                geom.vertices.emplace_back(n.x * radius, y, n.z * radius);
                geom.normals.emplace_back(n);
            }
        }

        for (int s = 0; s < sectors; ++s)
        {
            auto i0 = sideStart + (std::uint32_t)s;
            auto i1 = i0 + 1u;
            auto i2 = i0 + (std::uint32_t)sectors + 1u;
            auto i3 = i2 + 1u;
            geom.indices.insert(geom.indices.end(), { i0, i2, i1, i1, i2, i3 });
        }

        auto addCap = [&](double y, const glm::dvec3& normal, bool top)
            {
                auto center = (std::uint32_t)geom.vertices.size();
                geom.vertices.emplace_back(0.0, y, 0.0);
                geom.normals.emplace_back(normal);

                auto ringStart = (std::uint32_t)geom.vertices.size();
                for (int s = 0; s <= sectors; ++s)
                {
                    double phi = glm::two_pi<double>() * (double)s / (double)sectors;
                    geom.vertices.emplace_back(std::cos(phi) * radius, y, std::sin(phi) * radius);
                    geom.normals.emplace_back(normal);
                }

                for (int s = 0; s < sectors; ++s)
                {
                    auto a = ringStart + (std::uint32_t)s;
                    auto b = a + 1u;
                    if (top)
                        geom.indices.insert(geom.indices.end(), { center, b, a });
                    else
                        geom.indices.insert(geom.indices.end(), { center, a, b });
                }
            };

        addCap(halfHeight, { 0.0, 1.0, 0.0 }, true);
        addCap(-halfHeight, { 0.0, -1.0, 0.0 }, false);
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
        RigidBody::MotionType motion,
        const glm::dquat& rotation = glm::dquat(1.0, 0.0, 0.0, 0.0))
    {
        auto entity = registry.create();

        auto& geom = registry.emplace<MeshGeometry>(entity);
        PhysicsDemo_makeBox(geom, halfExtents);

        auto& style = registry.emplace<MeshStyle>(entity);
        style.color = color;
        style.lighting = true;

        registry.emplace<Mesh>(entity, geom, style);
        auto& transform = PhysicsDemo_addTransform(registry, entity, origin, offset);
        transform.localMatrix = glm::translate(glm::dmat4(1.0), offset) *
            glm::mat4_cast(glm::normalize(rotation));

        auto& body = registry.emplace<RigidBody>(entity);
        body.motion = motion;
        body.mass = (float)std::max(1.0, halfExtents.x * halfExtents.y * halfExtents.z * 8.0);
        body.material.friction = 0.65f;
        body.material.restitution = 0.15f;

        auto& box = registry.emplace<BoxCollider>(entity);
        box.halfExtents = halfExtents;

        return entity;
    }

    entt::entity PhysicsDemo_addCylinder(
        entt::registry& registry,
        const GeoPoint& origin,
        const glm::dvec3& offset,
        double radius,
        double halfHeight,
        const Color& color,
        const glm::dquat& rotation = glm::dquat(1.0, 0.0, 0.0, 0.0))
    {
        auto entity = registry.create();

        auto& geom = registry.emplace<MeshGeometry>(entity);
        PhysicsDemo_makeCylinder(geom, radius, halfHeight);

        auto& style = registry.emplace<MeshStyle>(entity);
        style.color = color;
        style.lighting = true;

        registry.emplace<Mesh>(entity, geom, style);
        auto& transform = PhysicsDemo_addTransform(registry, entity, origin, offset);
        transform.localMatrix = glm::translate(glm::dmat4(1.0), offset) *
            glm::mat4_cast(glm::normalize(rotation));

        auto& body = registry.emplace<RigidBody>(entity);
        body.motion = RigidBody::MotionType::Dynamic;
        body.mass = (float)std::max(1.0, radius * radius * halfHeight * 6.0);
        body.material.friction = 0.55f;
        body.material.restitution = 0.35f;

        auto& cylinder = registry.emplace<CylinderCollider>(entity);
        cylinder.radius = radius;
        cylinder.halfHeight = halfHeight;

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
        const Color& color,
        const glm::dquat& rotation = glm::angleAxis(glm::radians(25.0), glm::dvec3(0.0, 0.0, 1.0)))
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
            glm::mat4_cast(glm::normalize(rotation));

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

                entities.emplace_back(PhysicsDemo_addBox(
                    registry, origin, { -30.0, 34.0, 8.0 },
                    { 22.0, 7.0, 1.25 },
                    Color(0.40f, 0.46f, 0.52f, 1.0f),
                    RigidBody::MotionType::Static,
                    glm::angleAxis(glm::radians(-14.0), glm::dvec3(1.0, 0.0, 0.0))));

                entities.emplace_back(PhysicsDemo_addBox(
                    registry, origin, { 32.0, 38.0, 9.0 },
                    { 20.0, 6.0, 1.25 },
                    Color(0.34f, 0.42f, 0.48f, 1.0f),
                    RigidBody::MotionType::Static,
                    glm::angleAxis(glm::radians(13.0), glm::dvec3(1.0, 0.0, 0.0))));

                entities.emplace_back(PhysicsDemo_addBox(
                    registry, origin, { -52.0, -2.0, 14.0 },
                    { 2.0, 28.0, 7.0 },
                    Color(0.36f, 0.38f, 0.42f, 1.0f),
                    RigidBody::MotionType::Static,
                    glm::angleAxis(glm::radians(-24.0), glm::dvec3(0.0, 0.0, 1.0))));

                entities.emplace_back(PhysicsDemo_addBox(
                    registry, origin, { 52.0, 6.0, 14.0 },
                    { 2.0, 28.0, 7.0 },
                    Color(0.36f, 0.38f, 0.42f, 1.0f),
                    RigidBody::MotionType::Static,
                    glm::angleAxis(glm::radians(24.0), glm::dvec3(0.0, 0.0, 1.0))));

                const Color boxColors[] = {
                    Color(0.95f, 0.20f, 0.18f, 1.0f),
                    Color(0.10f, 0.65f, 1.00f, 1.0f),
                    Color(1.00f, 0.78f, 0.12f, 1.0f),
                    Color(0.20f, 0.90f, 0.35f, 1.0f),
                    Color(0.85f, 0.35f, 1.00f, 1.0f),
                    Color(0.00f, 0.85f, 0.78f, 1.0f)
                };

                for (int i = 0; i < 8; ++i)
                {
                    double x = -50.0 + i * 14.0;
                    double y = -18.0 + (i % 2) * 12.0;
                    double z = 26.0 + i * 9.0;
                    glm::dvec3 halfExtents =
                        i % 3 == 0 ? glm::dvec3(5.0, 3.0, 4.0) :
                        i % 3 == 1 ? glm::dvec3(3.25, 5.0, 3.25) :
                        glm::dvec3(4.5, 4.5, 4.5);
                    entities.emplace_back(PhysicsDemo_addBox(
                        registry, origin, { x, y, z },
                        halfExtents,
                        boxColors[i % (sizeof(boxColors) / sizeof(boxColors[0]))],
                        RigidBody::MotionType::Dynamic,
                        glm::angleAxis(glm::radians(i * 17.0), glm::dvec3(0.0, 0.0, 1.0)) *
                        glm::angleAxis(glm::radians(8.0 + i * 3.0), glm::dvec3(1.0, 0.0, 0.0))));
                }

                for (int level = 0; level < 3; ++level)
                {
                    for (int col = 0; col < 3; ++col)
                    {
                        entities.emplace_back(PhysicsDemo_addBox(
                            registry, origin, { -15.0 + col * 7.0, -46.0, 11.0 + level * 7.2 },
                            { 3.0, 3.0, 3.0 },
                            boxColors[(level * 3 + col) % (sizeof(boxColors) / sizeof(boxColors[0]))],
                            RigidBody::MotionType::Dynamic,
                            glm::angleAxis(glm::radians((level + col) * 8.0), glm::dvec3(0.0, 0.0, 1.0))));
                    }
                }

                const Color sphereColors[] = {
                    Color(1.00f, 0.42f, 0.12f, 1.0f),
                    Color(0.35f, 0.85f, 1.00f, 1.0f),
                    Color(0.70f, 0.95f, 0.25f, 1.0f),
                    Color(1.00f, 0.35f, 0.62f, 1.0f)
                };

                for (int i = 0; i < 6; ++i)
                {
                    double x = -38.0 + i * 15.0;
                    double y = 16.0 + (i % 2) * 16.0;
                    double z = 36.0 + i * 12.0;
                    entities.emplace_back(PhysicsDemo_addSphere(
                        registry, origin, { x, y, z },
                        i % 2 == 0 ? 4.2 : 3.5,
                        sphereColors[i % (sizeof(sphereColors) / sizeof(sphereColors[0]))]));
                }

                const Color cylinderColors[] = {
                    Color(0.95f, 0.82f, 0.18f, 1.0f),
                    Color(0.18f, 0.75f, 0.95f, 1.0f),
                    Color(0.95f, 0.28f, 0.38f, 1.0f),
                    Color(0.50f, 0.92f, 0.42f, 1.0f)
                };

                for (int i = 0; i < 6; ++i)
                {
                    double x = -44.0 + i * 17.0;
                    double y = 44.0 + (i % 3 - 1) * 11.0;
                    double z = 58.0 + i * 8.0;
                    entities.emplace_back(PhysicsDemo_addCylinder(
                        registry, origin, { x, y, z },
                        3.4, 5.5,
                        cylinderColors[i % (sizeof(cylinderColors) / sizeof(cylinderColors[0]))],
                        glm::angleAxis(glm::radians(i * 21.0), glm::dvec3(0.0, 0.0, 1.0))));
                }

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { -24.0, 38.0, 50.0 },
                    3.5, 7.0,
                    Color(1.00f, 0.35f, 0.75f, 1.0f),
                    glm::angleAxis(glm::radians(25.0), glm::dvec3(0.0, 0.0, 1.0))));

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { 24.0, 38.0, 66.0 },
                    3.5, 7.0,
                    Color(0.35f, 1.00f, 0.95f, 1.0f),
                    glm::angleAxis(glm::radians(-30.0), glm::dvec3(0.0, 0.0, 1.0))));

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { -44.0, -34.0, 62.0 },
                    3.0, 6.0,
                    Color(0.95f, 0.60f, 0.25f, 1.0f),
                    glm::angleAxis(glm::radians(60.0), glm::dvec3(0.0, 0.0, 1.0))));

                entities.emplace_back(PhysicsDemo_addCapsule(
                    registry, origin, { 42.0, -32.0, 78.0 },
                    3.0, 6.0,
                    Color(0.65f, 0.55f, 1.00f, 1.0f),
                    glm::angleAxis(glm::radians(-58.0), glm::dvec3(0.0, 0.0, 1.0))));
            });

        auto& view = app.display.window(0).view(0);
        if (auto manip = MapManipulator::get(view.vsgView))
        {
            Viewpoint vp;
            vp.point = origin;
            vp.range = 560.0;
            vp.pitch = -38.0;
            vp.heading = 25.0;
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
                if (ImGuiLTable::SliderFloat("Gravity", &gravity, -30.0f,30.0f, "%.1f m/s^2"))
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
