/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "JoltPhysicsSystem.h"

#include "ECSTypes.h"
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Transform.h>
#include <rocky/ecs/Visibility.h>
#include <rocky/vsg/VSGUtils.h>
#include <rocky/vsg/terrain/SurfaceNode.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <rocky/vsg/terrain/TerrainTileNode.h>
#include <rocky/Color.h>
#include <rocky/Math.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#ifdef ROCKY_HAS_JOLT

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#define ROCKY_JOLT_VERSION_STRING ROCKY_STR(JPH_VERSION_MAJOR) "." ROCKY_STR(JPH_VERSION_MINOR) "." ROCKY_STR(JPH_VERSION_PATCH)
ROCKY_ABOUT(jolt, ROCKY_JOLT_VERSION_STRING);

#endif

using namespace ROCKY_NAMESPACE;

#ifdef ROCKY_HAS_JOLT

namespace
{
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS = 2;
    }

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
        {
            if (a == Layers::NON_MOVING)
                return b == Layers::MOVING;
            if (a == Layers::MOVING)
                return true;
            return false;
        }
    };

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            _objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            _objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            return _objectToBroadPhase[layer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
        {
            if (layer == BroadPhaseLayers::NON_MOVING) return "NON_MOVING";
            if (layer == BroadPhaseLayers::MOVING) return "MOVING";
            return "INVALID";
        }
#endif

    private:
        JPH::BroadPhaseLayer _objectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
        {
            if (layer == Layers::NON_MOVING)
                return broadPhaseLayer == BroadPhaseLayers::MOVING;
            if (layer == Layers::MOVING)
                return true;
            return false;
        }
    };

    struct JoltGlobals
    {
        JoltGlobals()
        {
            std::scoped_lock lock(mutex());
            if (refCount()++ == 0)
            {
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }
        }

        ~JoltGlobals()
        {
            std::scoped_lock lock(mutex());
            if (--refCount() == 0)
            {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
            }
        }

        static std::mutex& mutex()
        {
            static std::mutex s_mutex;
            return s_mutex;
        }

        static int& refCount()
        {
            static int s_refCount = 0;
            return s_refCount;
        }
    };

    struct JoltRigidBodyDetail
    {
        JPH::BodyID bodyID;
        entt::entity debugEntity = entt::null;
        int transformRevision = -1;
        bool rebuild = true;
        RigidBody::MotionType motion = RigidBody::MotionType::Dynamic;

        bool valid() const { return !bodyID.IsInvalid(); }
    };

    struct JoltDestructionQueue
    {
        std::mutex mutex;
        std::vector<JPH::BodyID> bodies;
        std::vector<entt::entity> debugEntities;
    };

    JPH::Vec3 toJoltVec3(const glm::dvec3& v)
    {
        return JPH::Vec3((float)v.x, (float)v.y, (float)v.z);
    }

    JPH::RVec3 toJoltRVec3(const glm::dvec3& v)
    {
        return JPH::RVec3((JPH::Real)v.x, (JPH::Real)v.y, (JPH::Real)v.z);
    }

    glm::dvec3 fromJoltVec3(const JPH::Vec3& v)
    {
        return { v.GetX(), v.GetY(), v.GetZ() };
    }

    glm::dvec3 fromJoltRVec3(const JPH::RVec3& v)
    {
        return { (double)v.GetX(), (double)v.GetY(), (double)v.GetZ() };
    }

    JPH::Quat toJoltQuat(const glm::dquat& q)
    {
        auto n = glm::normalize(q);
        return JPH::Quat((float)n.x, (float)n.y, (float)n.z, (float)n.w);
    }

    glm::dquat fromJoltQuat(const JPH::Quat& q)
    {
        return glm::normalize(glm::dquat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()));
    }

    JPH::EMotionType toJoltMotion(RigidBody::MotionType motion)
    {
        if (motion == RigidBody::MotionType::Static)
            return JPH::EMotionType::Static;
        if (motion == RigidBody::MotionType::Kinematic)
            return JPH::EMotionType::Kinematic;
        return JPH::EMotionType::Dynamic;
    }

    JPH::ObjectLayer toJoltLayer(RigidBody::MotionType motion)
    {
        return motion == RigidBody::MotionType::Dynamic ? Layers::MOVING : Layers::NON_MOVING;
    }

    bool poseIsIdentity(const ColliderPose& pose)
    {
        return
            glm::dot(pose.offset, pose.offset) < 1e-12 &&
            std::abs(glm::dot(glm::normalize(pose.rotation), glm::dquat(1.0, 0.0, 0.0, 0.0))) > 0.999999;
    }

    template<class SHAPE_SETTINGS>
    JPH::ShapeRefC createShape(SHAPE_SETTINGS& settings, std::string& error)
    {
        settings.SetEmbedded();
        auto result = settings.Create();
        if (result.HasError())
        {
            error = result.GetError().c_str();
            return { };
        }
        return result.Get();
    }

    struct ShapeBuildResult
    {
        JPH::ShapeRefC shape;
        bool mustBeStatic = false;
        std::string error;
    };

    void fillBoxMesh(MeshGeometry& geom, const glm::dvec3& halfExtents)
    {
        const auto& h = halfExtents;
        geom.vertices = {
            {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
            {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z}
        };
        geom.indices = {
            0, 3, 2, 0, 2, 1, 4, 5, 6, 4, 6, 7,
            1, 2, 6, 1, 6, 5, 3, 0, 4, 3, 4, 7,
            0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6
        };
    }

    void fillSphereMesh(MeshGeometry& geom, double radius, int rings = 12, int sectors = 24)
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

    void fillCylinderMesh(MeshGeometry& geom, double radius, double halfHeight, int sectors = 24)
    {
        const std::uint32_t bottomCenter = 0;
        const std::uint32_t topCenter = 1;
        geom.vertices.emplace_back(0.0, -halfHeight, 0.0);
        geom.vertices.emplace_back(0.0, halfHeight, 0.0);

        for (int s = 0; s <= sectors; ++s)
        {
            double a = glm::two_pi<double>() * (double)s / (double)sectors;
            double x = std::cos(a) * radius;
            double z = std::sin(a) * radius;
            geom.vertices.emplace_back(x, -halfHeight, z);
            geom.vertices.emplace_back(x, halfHeight, z);
        }

        for (int s = 0; s < sectors; ++s)
        {
            std::uint32_t b0 = 2 + s * 2;
            std::uint32_t t0 = b0 + 1;
            std::uint32_t b1 = b0 + 2;
            std::uint32_t t1 = b1 + 1;
            geom.indices.insert(geom.indices.end(), { b0, b1, t1, b0, t1, t0 });
            geom.indices.insert(geom.indices.end(), { bottomCenter, b0, b1, topCenter, t1, t0 });
        }
    }

    void fillCapsuleMesh(MeshGeometry& geom, double radius, double halfHeight, int rings = 12, int sectors = 24)
    {
        for (int r = 0; r <= rings; ++r)
        {
            double v = (double)r / (double)rings;
            double theta = glm::pi<double>() * v;
            double y = std::cos(theta) * radius;
            double capOffset = y >= 0.0 ? halfHeight : -halfHeight;
            double rr = std::sin(theta) * radius;

            for (int s = 0; s <= sectors; ++s)
            {
                double phi = glm::two_pi<double>() * (double)s / (double)sectors;
                geom.vertices.emplace_back(std::cos(phi) * rr, y + capOffset, std::sin(phi) * rr);
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

    void applyColliderPose(MeshGeometry& geom, const ColliderPose& pose)
    {
        if (geom.vertices.empty() || poseIsIdentity(pose))
            return;

        glm::dmat4 matrix =
            glm::translate(glm::dmat4(1.0), pose.offset) *
            glm::mat4_cast(glm::normalize(pose.rotation));

        for (auto& vertex : geom.vertices)
            vertex = matrix * glm::dvec4(vertex, 1.0);

        for (auto& normal : geom.normals)
            normal = glm::dmat3(matrix) * glm::dvec3(normal);
    }

    void markRigidBodyDirty(entt::registry& registry, entt::entity entity)
    {
        if (auto* detail = registry.try_get<JoltRigidBodyDetail>(entity))
            detail->rebuild = true;

        if (registry.all_of<RigidBody>(entity))
            RigidBody::dirty(registry, entity);
    }

    void on_construct_RigidBody(entt::registry& registry, entt::entity entity)
    {
        (void)registry.get_or_emplace<JoltRigidBodyDetail>(entity);
        RigidBody::dirty(registry, entity);
    }

    void on_update_RigidBody(entt::registry& registry, entt::entity entity)
    {
        markRigidBodyDirty(registry, entity);
    }

    void on_destroy_RigidBody(entt::registry& registry, entt::entity entity)
    {
        auto* detail = registry.try_get<JoltRigidBodyDetail>(entity);
        if (!detail)
            return;

        registry.view<JoltDestructionQueue>().each([&](JoltDestructionQueue& queue)
            {
                std::scoped_lock lock(queue.mutex);
                if (detail->valid())
                    queue.bodies.emplace_back(detail->bodyID);
                if (detail->debugEntity != entt::null)
                    queue.debugEntities.emplace_back(detail->debugEntity);
            });
    }

    void on_construct_BoxCollider(entt::registry& r, entt::entity e) { BoxCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_construct_SphereCollider(entt::registry& r, entt::entity e) { SphereCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_construct_CapsuleCollider(entt::registry& r, entt::entity e) { CapsuleCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_construct_CylinderCollider(entt::registry& r, entt::entity e) { CylinderCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_construct_MeshCollider(entt::registry& r, entt::entity e) { MeshCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_construct_ConvexHullCollider(entt::registry& r, entt::entity e) { ConvexHullCollider::dirty(r, e); markRigidBodyDirty(r, e); }

    void on_update_BoxCollider(entt::registry& r, entt::entity e) { BoxCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_update_SphereCollider(entt::registry& r, entt::entity e) { SphereCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_update_CapsuleCollider(entt::registry& r, entt::entity e) { CapsuleCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_update_CylinderCollider(entt::registry& r, entt::entity e) { CylinderCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_update_MeshCollider(entt::registry& r, entt::entity e) { MeshCollider::dirty(r, e); markRigidBodyDirty(r, e); }
    void on_update_ConvexHullCollider(entt::registry& r, entt::entity e) { ConvexHullCollider::dirty(r, e); markRigidBodyDirty(r, e); }

    void on_destroy_BoxCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
    void on_destroy_SphereCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
    void on_destroy_CapsuleCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
    void on_destroy_CylinderCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
    void on_destroy_MeshCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
    void on_destroy_ConvexHullCollider(entt::registry& r, entt::entity e) { markRigidBodyDirty(r, e); }
}

struct JoltPhysicsSystem::Impl
{
    Registry registry;
    vsg::observer_ptr<TerrainNode> terrain;
    CallbackSubs terrainSubscriptions;

    std::unique_ptr<JoltGlobals> globals;
    std::unique_ptr<JPH::PhysicsSystem> physics;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;

    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;

    PhysicsSettings settings;
    entt::entity settingsEntity = entt::null;

    vsg::time_point lastUpdateTime = vsg::time_point::min();
    double accumulator = 0.0;

    struct Island
    {
        bool valid = false;
        SRS worldSRS = SRS::ECEF;
        glm::dvec3 originWorld = { 0.0, 0.0, 0.0 };
        glm::dmat4 islandToWorld = glm::dmat4(1.0);
        glm::dmat4 worldToIsland = glm::dmat4(1.0);
    };
    Island island;

    struct TerrainBody
    {
        JPH::BodyID bodyID;
        entt::entity debugEntity = entt::null;
        vsg::observer_ptr<TerrainTileNode> tile;
    };

    std::map<TileKey, TerrainBody> terrainBodies;
    std::map<TileKey, vsg::observer_ptr<TerrainTileNode>> terrainResidentTiles;
    std::mutex terrainQueueMutex;
    std::vector<vsg::observer_ptr<TerrainTileNode>> terrainUpserts;
    std::vector<TileKey> terrainRemoves;

    bool rebuildAllBodies = false;

    Impl(Registry in_registry) : registry(in_registry) { }

    JPH::BodyInterface& bodies()
    {
        return physics->GetBodyInterface();
    }

    void initializePhysics(const PhysicsSettings& in_settings)
    {
        if (physics)
            return;

        globals = std::make_unique<JoltGlobals>();
        tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(in_settings.tempAllocatorSizeMB * 1024u * 1024u);

        const JPH::uint threadCount = std::max<JPH::uint>(1u, std::thread::hardware_concurrency());
        jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            threadCount > 1u ? threadCount - 1u : 1u);

        physics = std::make_unique<JPH::PhysicsSystem>();
        physics->Init(
            in_settings.maxBodies,
            in_settings.numBodyMutexes,
            in_settings.maxBodyPairs,
            in_settings.maxContactConstraints,
            broadPhaseLayerInterface,
            objectVsBroadPhaseLayerFilter,
            objectLayerPairFilter);

        physics->SetGravity(toJoltVec3(in_settings.gravity));
    }

    void shutdownPhysics(entt::registry* reg)
    {
        if (!physics)
            return;

        if (reg)
        {
            reg->view<JoltRigidBodyDetail>().each([&](JoltRigidBodyDetail& detail)
                {
                    destroyBody(detail.bodyID);
                    if (detail.debugEntity != entt::null && reg->valid(detail.debugEntity))
                        reg->destroy(detail.debugEntity);
                    detail.debugEntity = entt::null;
                });

            for (auto& [key, terrainBody] : terrainBodies)
            {
                destroyBody(terrainBody.bodyID);
                if (terrainBody.debugEntity != entt::null && reg->valid(terrainBody.debugEntity))
                    reg->destroy(terrainBody.debugEntity);
            }
        }
        else
        {
            for (auto& [key, terrainBody] : terrainBodies)
                destroyBody(terrainBody.bodyID);
        }

        terrainBodies.clear();
        physics.reset();
        jobSystem.reset();
        tempAllocator.reset();
        globals.reset();
    }

    void destroyBody(JPH::BodyID& bodyID)
    {
        if (physics && !bodyID.IsInvalid())
        {
            bodies().RemoveBody(bodyID);
            bodies().DestroyBody(bodyID);
            bodyID = JPH::BodyID();
        }
    }

    SRS chooseWorldSRS(entt::registry& reg)
    {
        if (auto strong = terrain.ref_ptr())
        {
            if (strong->renderingSRS.valid())
                return strong->renderingSRS;
        }

        SRS found;
        reg.view<RigidBody, Transform>().each([&](auto, RigidBody&, Transform& transform)
            {
                if (!found.valid() && transform.position.valid())
                    found = transform.position.srs.isGeocentric() ? transform.position.srs : transform.position.srs.geocentricSRS();
            });

        return found.valid() ? found : SRS::ECEF;
    }

    bool transformToWorldModel(const Transform& transform, glm::dmat4& output) const
    {
        if (!transform.position.valid() || !island.worldSRS.valid())
            return false;

        GeoPoint worldPoint = transform.position.transform(island.worldSRS);
        glm::dvec3 world(worldPoint.x, worldPoint.y, worldPoint.z);

        glm::dmat4 base =
            transform.topocentric ?
            island.worldSRS.topocentricToWorldMatrix(world) :
            glm::translate(glm::dmat4(1.0), world);

        output = base * transform.localMatrix;
        return true;
    }

    bool transformToIslandPose(const Transform& transform, glm::dvec3& position, glm::dquat& rotation) const
    {
        glm::dmat4 worldModel;
        if (!transformToWorldModel(transform, worldModel))
            return false;

        glm::dmat4 islandModel = island.worldToIsland * worldModel;
        position = glm::dvec3(islandModel[3]);
        rotation = quaternion_from_matrix<glm::dquat>(islandModel);
        return true;
    }

    void applyIslandPoseToTransform(
        const glm::dvec3& islandPosition,
        const glm::dquat& islandRotation,
        Transform& transform,
        entt::registry& reg,
        JoltRigidBodyDetail& detail)
    {
        glm::dmat4 islandModel =
            glm::translate(glm::dmat4(1.0), islandPosition) *
            glm::mat4_cast(glm::normalize(islandRotation));

        glm::dmat4 worldModel = island.islandToWorld * islandModel;
        glm::dvec3 worldPos(worldModel[3]);

        SRS outputSRS = transform.position.srs.valid() ? transform.position.srs : island.worldSRS;
        transform.position = GeoPoint(island.worldSRS, worldPos).transform(outputSRS);

        glm::dmat4 base =
            transform.topocentric ?
            island.worldSRS.topocentricToWorldMatrix(worldPos) :
            glm::translate(glm::dmat4(1.0), worldPos);

        transform.localMatrix = glm::inverse(base) * worldModel;
        transform.dirty(reg);
        detail.transformRevision = transform.revision;
    }

    bool ensureIsland(entt::registry& reg)
    {
        SRS worldSRS = chooseWorldSRS(reg);
        if (!worldSRS.valid())
            return false;

        bool srsChanged = island.valid && island.worldSRS != worldSRS;
        if (island.valid && !srsChanged)
            return true;

        glm::dvec3 origin(0.0);
        bool found = false;
        reg.view<RigidBody, Transform>().each([&](auto, RigidBody&, Transform& transform)
            {
                if (!found && transform.position.valid())
                {
                    auto p = transform.position.transform(worldSRS);
                    origin = glm::dvec3(p.x, p.y, p.z);
                    found = true;
                }
            });

        if (!found)
            origin = worldSRS.isGeocentric() ? glm::dvec3(worldSRS.ellipsoid().semiMajorAxis(), 0.0, 0.0) : glm::dvec3(0.0);

        setIsland(worldSRS, origin);
        rebuildAllBodies = true;
        return true;
    }

    void setIsland(const SRS& worldSRS, const glm::dvec3& originWorld)
    {
        island.valid = true;
        island.worldSRS = worldSRS;
        island.originWorld = originWorld;
        island.islandToWorld = worldSRS.topocentricToWorldMatrix(originWorld);
        island.worldToIsland = glm::inverse(island.islandToWorld);
    }

    bool shouldRebase(entt::registry& reg)
    {
        if (!island.valid || settings.islandRebaseDistance <= 0.0)
            return false;

        bool rebase = false;
        reg.view<RigidBody, Transform>().each([&](auto, RigidBody&, Transform& transform)
            {
                if (!rebase && transform.position.valid())
                {
                    auto p = transform.position.transform(island.worldSRS);
                    auto local = island.worldToIsland * glm::dvec4(p.x, p.y, p.z, 1.0);
                    rebase = glm::length(glm::dvec3(local)) > settings.islandRebaseDistance;
                }
            });
        return rebase;
    }

    void rebaseIsland(entt::registry& reg)
    {
        glm::dvec3 origin = island.originWorld;
        bool found = false;
        reg.view<RigidBody, Transform>().each([&](auto, RigidBody&, Transform& transform)
            {
                if (!found && transform.position.valid())
                {
                    auto p = transform.position.transform(island.worldSRS);
                    origin = glm::dvec3(p.x, p.y, p.z);
                    found = true;
                }
            });

        setIsland(island.worldSRS, origin);
        rebuildAllBodies = true;
    }

    void drainDestructionQueue(entt::registry& reg)
    {
        std::vector<JPH::BodyID> bodiesToDestroy;
        std::vector<entt::entity> debugToDestroy;

        reg.view<JoltDestructionQueue>().each([&](JoltDestructionQueue& queue)
            {
                std::scoped_lock lock(queue.mutex);
                bodiesToDestroy.swap(queue.bodies);
                debugToDestroy.swap(queue.debugEntities);
            });

        for (auto& bodyID : bodiesToDestroy)
            destroyBody(bodyID);

        for (auto entity : debugToDestroy)
            if (reg.valid(entity))
                reg.destroy(entity);
    }

    void processDirtyLists(entt::registry& reg)
    {
        RigidBody::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        BoxCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        SphereCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        CapsuleCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        CylinderCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        MeshCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
        ConvexHullCollider::eachDirty(reg, [&](entt::entity e) { if (auto* d = reg.try_get<JoltRigidBodyDetail>(e)) d->rebuild = true; });
    }

    ShapeBuildResult buildShape(entt::registry& reg, entt::entity entity)
    {
        struct SubShape
        {
            JPH::ShapeRefC shape;
            ColliderPose pose;
        };

        ShapeBuildResult result;
        std::vector<SubShape> shapes;

        auto add = [&](JPH::ShapeRefC shape, const ColliderPose& pose)
            {
                if (shape)
                {
                    result.mustBeStatic = result.mustBeStatic || shape->MustBeStatic();
                    shapes.push_back({ shape, pose });
                }
            };

        if (auto* collider = reg.try_get<BoxCollider>(entity); collider && collider->pose.enabled)
        {
            JPH::BoxShapeSettings settings(
                toJoltVec3(glm::max(collider->halfExtents, glm::dvec3(0.001))),
                std::max(collider->convexRadius, 0.0f));
            add(createShape(settings, result.error), collider->pose);
        }

        if (auto* collider = reg.try_get<SphereCollider>(entity); collider && collider->pose.enabled)
        {
            JPH::SphereShapeSettings settings((float)std::max(collider->radius, 0.001));
            add(createShape(settings, result.error), collider->pose);
        }

        if (auto* collider = reg.try_get<CapsuleCollider>(entity); collider && collider->pose.enabled)
        {
            JPH::CapsuleShapeSettings settings(
                (float)std::max(collider->halfHeightOfCylinder, 0.0),
                (float)std::max(collider->radius, 0.001));
            add(createShape(settings, result.error), collider->pose);
        }

        if (auto* collider = reg.try_get<CylinderCollider>(entity); collider && collider->pose.enabled)
        {
            JPH::CylinderShapeSettings settings(
                (float)std::max(collider->halfHeight, 0.001),
                (float)std::max(collider->radius, 0.001),
                std::max(collider->convexRadius, 0.0f));
            add(createShape(settings, result.error), collider->pose);
        }

        if (auto* collider = reg.try_get<MeshCollider>(entity); collider && collider->pose.enabled)
        {
            if (collider->indices.size() >= 3 && !collider->vertices.empty())
            {
                JPH::TriangleList triangles;
                triangles.reserve(collider->indices.size() / 3);
                for (std::size_t i = 0; i + 2 < collider->indices.size(); i += 3)
                {
                    auto i0 = collider->indices[i];
                    auto i1 = collider->indices[i + 1];
                    auto i2 = collider->indices[i + 2];
                    if (i0 < collider->vertices.size() && i1 < collider->vertices.size() && i2 < collider->vertices.size())
                    {
                        triangles.emplace_back(
                            toJoltVec3(collider->vertices[i0]),
                            toJoltVec3(collider->vertices[i1]),
                            toJoltVec3(collider->vertices[i2]));
                    }
                }

                if (!triangles.empty())
                {
                    JPH::MeshShapeSettings settings(triangles);
                    add(createShape(settings, result.error), collider->pose);
                }
            }
        }

        if (auto* collider = reg.try_get<ConvexHullCollider>(entity); collider && collider->pose.enabled)
        {
            if (collider->points.size() >= 4)
            {
                JPH::Array<JPH::Vec3> points;
                points.reserve(collider->points.size());
                for (auto& point : collider->points)
                    points.emplace_back(toJoltVec3(point));

                JPH::ConvexHullShapeSettings settings(points, std::max(collider->convexRadius, 0.0f));
                add(createShape(settings, result.error), collider->pose);
            }
        }

        if (!result.error.empty() || shapes.empty())
            return result;

        if (shapes.size() == 1 && poseIsIdentity(shapes.front().pose))
        {
            result.shape = shapes.front().shape;
            return result;
        }

        JPH::StaticCompoundShapeSettings compound;
        compound.SetEmbedded();
        for (auto& subShape : shapes)
        {
            compound.AddShape(
                toJoltVec3(subShape.pose.offset),
                toJoltQuat(subShape.pose.rotation),
                subShape.shape.GetPtr());
        }

        auto compoundResult = compound.Create();
        if (compoundResult.HasError())
            result.error = compoundResult.GetError().c_str();
        else
            result.shape = compoundResult.Get();

        return result;
    }

    entt::entity createOrUpdateDebugMesh(
        entt::registry& reg,
        entt::entity debugEntity,
        const MeshGeometry& sourceGeometry,
        const Transform* sourceTransform)
    {
        if (debugEntity == entt::null || !reg.valid(debugEntity))
            debugEntity = reg.create();

        auto& geom = reg.get_or_emplace<MeshGeometry>(debugEntity);
        geom.vertices = sourceGeometry.vertices;
        geom.indices = sourceGeometry.indices;
        geom.normals = sourceGeometry.normals;
        geom.colors.clear();
        geom.uvs.clear();
        geom.srs = sourceGeometry.srs;
        geom.owner = debugEntity;
        geom.dirty(reg);

        auto& style = reg.get_or_emplace<MeshStyle>(debugEntity);
        style.color = Color(1.0f, 1.0f, 0.0f, 0.85f);
        style.wireframe = true;
        style.writeDepth = false;
        style.drawBackfaces = true;
        style.dirty(reg);

        auto& mesh = reg.get_or_emplace<Mesh>(debugEntity);
        mesh.geometry = debugEntity;
        mesh.style = debugEntity;
        mesh.dirty(reg);

        if (sourceTransform)
        {
            auto& transform = reg.get_or_emplace<Transform>(debugEntity);
            transform = *sourceTransform;
            transform.owner = debugEntity;
            transform.dirty(reg);
        }
        else if (reg.all_of<Transform>(debugEntity))
        {
            reg.remove<Transform>(debugEntity);
        }

        return debugEntity;
    }

    MeshGeometry createDebugGeometry(entt::registry& reg, entt::entity entity)
    {
        MeshGeometry geom;

        if (auto* collider = reg.try_get<BoxCollider>(entity); collider && collider->pose.enabled)
        {
            fillBoxMesh(geom, collider->halfExtents);
            applyColliderPose(geom, collider->pose);
        }
        else if (auto* collider = reg.try_get<SphereCollider>(entity); collider && collider->pose.enabled)
        {
            fillSphereMesh(geom, collider->radius);
            applyColliderPose(geom, collider->pose);
        }
        else if (auto* collider = reg.try_get<CapsuleCollider>(entity); collider && collider->pose.enabled)
        {
            fillCapsuleMesh(geom, collider->radius, collider->halfHeightOfCylinder);
            applyColliderPose(geom, collider->pose);
        }
        else if (auto* collider = reg.try_get<CylinderCollider>(entity); collider && collider->pose.enabled)
        {
            fillCylinderMesh(geom, collider->radius, collider->halfHeight);
            applyColliderPose(geom, collider->pose);
        }
        else if (auto* collider = reg.try_get<MeshCollider>(entity); collider && collider->pose.enabled)
        {
            geom.vertices = collider->vertices;
            geom.indices = collider->indices;
            applyColliderPose(geom, collider->pose);
        }
        else if (auto* collider = reg.try_get<ConvexHullCollider>(entity); collider && collider->pose.enabled && !collider->points.empty())
        {
            glm::dvec3 minv(DBL_MAX), maxv(-DBL_MAX);
            for (auto& point : collider->points)
            {
                minv = glm::min(minv, point);
                maxv = glm::max(maxv, point);
            }
            fillBoxMesh(geom, (maxv - minv) * 0.5);
            for (auto& vertex : geom.vertices)
                vertex += (minv + maxv) * 0.5;
            applyColliderPose(geom, collider->pose);
        }

        return geom;
    }

    void updateBodyDebugMesh(entt::registry& reg, entt::entity entity, JoltRigidBodyDetail& detail, bool enabled)
    {
        if (!enabled)
        {
            if (detail.debugEntity != entt::null && reg.valid(detail.debugEntity))
                reg.destroy(detail.debugEntity);
            detail.debugEntity = entt::null;
            return;
        }

        auto geom = createDebugGeometry(reg, entity);
        if (geom.vertices.empty() || geom.indices.empty())
            return;

        auto* transform = reg.try_get<Transform>(entity);
        detail.debugEntity = createOrUpdateDebugMesh(reg, detail.debugEntity, geom, transform);
    }

    void createOrUpdateBody(entt::registry& reg, entt::entity entity, RigidBody& body, Transform& transform, JoltRigidBodyDetail& detail)
    {
        if (!ensureIsland(reg))
            return;

        glm::dvec3 localPosition;
        glm::dquat localRotation;
        if (!transformToIslandPose(transform, localPosition, localRotation))
            return;

        auto shapeResult = buildShape(reg, entity);
        if (!shapeResult.error.empty())
        {
            Log()->warn("Jolt shape creation failed: {}", shapeResult.error);
            return;
        }
        if (!shapeResult.shape)
            return;

        destroyBody(detail.bodyID);

        RigidBody::MotionType motion = shapeResult.mustBeStatic ? RigidBody::MotionType::Static : body.motion;

        JPH::BodyCreationSettings settings(
            shapeResult.shape,
            toJoltRVec3(localPosition),
            toJoltQuat(localRotation),
            toJoltMotion(motion),
            toJoltLayer(motion));

        settings.mFriction = body.material.friction;
        settings.mRestitution = body.material.restitution;
        settings.mLinearDamping = body.linearDamping;
        settings.mAngularDamping = body.angularDamping;
        settings.mGravityFactor = body.gravityFactor;
        settings.mAllowSleeping = body.allowSleeping;
        settings.mIsSensor = body.sensor;
        settings.mEnhancedInternalEdgeRemoval = true;
        settings.mLinearVelocity = toJoltVec3(body.linearVelocity);
        settings.mAngularVelocity = toJoltVec3(body.angularVelocity);
        settings.mUserData = (std::uint64_t)entt::to_integral(entity);

        if (motion == RigidBody::MotionType::Dynamic && body.mass > 0.0f)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = body.mass;
        }

        detail.bodyID = bodies().CreateAndAddBody(
            settings,
            motion == RigidBody::MotionType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        detail.motion = motion;
        detail.transformRevision = transform.revision;
        detail.rebuild = false;
    }

    void syncBodyFromTransform(entt::entity entity, RigidBody& body, Transform& transform, JoltRigidBodyDetail& detail)
    {
        if (!detail.valid() || transform.revision == detail.transformRevision)
            return;

        glm::dvec3 localPosition;
        glm::dquat localRotation;
        if (!transformToIslandPose(transform, localPosition, localRotation))
            return;

        if (body.motion == RigidBody::MotionType::Kinematic)
        {
            bodies().MoveKinematic(
                detail.bodyID,
                toJoltRVec3(localPosition),
                toJoltQuat(localRotation),
                (float)std::max(settings.fixedTimeStep, 1.0 / 120.0));
        }
        else
        {
            bodies().SetPositionAndRotation(
                detail.bodyID,
                toJoltRVec3(localPosition),
                toJoltQuat(localRotation),
                JPH::EActivation::Activate);
        }

        detail.transformRevision = transform.revision;
    }

    void syncTransformFromBody(entt::registry& reg, RigidBody& body, Transform& transform, JoltRigidBodyDetail& detail)
    {
        if (!detail.valid() || detail.motion != RigidBody::MotionType::Dynamic)
            return;

        JPH::RVec3 position;
        JPH::Quat rotation;
        bodies().GetPositionAndRotation(detail.bodyID, position, rotation);

        body.linearVelocity = fromJoltVec3(bodies().GetLinearVelocity(detail.bodyID));
        body.angularVelocity = fromJoltVec3(bodies().GetAngularVelocity(detail.bodyID));

        applyIslandPoseToTransform(fromJoltRVec3(position), fromJoltQuat(rotation), transform, reg, detail);
    }

    void rebuildBodiesIfNeeded(entt::registry& reg)
    {
        if (rebuildAllBodies)
        {
            reg.view<JoltRigidBodyDetail>().each([&](JoltRigidBodyDetail& detail)
                {
                    destroyBody(detail.bodyID);
                    detail.rebuild = true;
                });

            for (auto& [key, terrainBody] : terrainBodies)
                destroyBody(terrainBody.bodyID);
            terrainBodies.clear();

            rebuildAllBodies = false;
        }

        reg.view<RigidBody, Transform, JoltRigidBodyDetail>().each(
            [&](entt::entity entity, RigidBody& body, Transform& transform, JoltRigidBodyDetail& detail)
            {
                if (detail.rebuild || !detail.valid())
                    createOrUpdateBody(reg, entity, body, transform, detail);
                else
                    syncBodyFromTransform(entity, body, transform, detail);

                updateBodyDebugMesh(reg, entity, detail, settings.debugDrawColliders);
            });
    }

    MeshGeometry createTerrainDebugGeometry(TerrainTileNode& tile)
    {
        MeshGeometry geom;
        std::vector<glm::dvec3> localVertices;
        if (!tile.surface || !tile.surface->copyColliderMesh(localVertices, geom.indices))
            return geom;

        geom.srs = island.worldSRS;
        geom.vertices.reserve(localVertices.size());
        for (auto& local : localVertices)
        {
            auto world = tile.surface->matrix * vsg::dvec4(local.x, local.y, local.z, 1.0);
            geom.vertices.emplace_back(world.x, world.y, world.z);
        }
        return geom;
    }

    JPH::ShapeRefC createTerrainShape(TerrainTileNode& tile)
    {
        std::vector<glm::dvec3> localVertices;
        std::vector<std::uint32_t> indices;
        if (!tile.surface || !tile.surface->copyColliderMesh(localVertices, indices))
            return { };

        JPH::TriangleList triangles;
        triangles.reserve(indices.size() / 3);

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            auto getIslandVertex = [&](std::uint32_t index)
                {
                    const auto& local = localVertices[index];
                    auto world = tile.surface->matrix * vsg::dvec4(local.x, local.y, local.z, 1.0);
                    auto islandVertex = island.worldToIsland * glm::dvec4(world.x, world.y, world.z, 1.0);
                    return toJoltVec3(glm::dvec3(islandVertex));
                };

            if (indices[i] < localVertices.size() &&
                indices[i + 1] < localVertices.size() &&
                indices[i + 2] < localVertices.size())
            {
                triangles.emplace_back(
                    getIslandVertex(indices[i]),
                    getIslandVertex(indices[i + 1]),
                    getIslandVertex(indices[i + 2]));
            }
        }

        if (triangles.empty())
            return { };

        std::string error;
        JPH::MeshShapeSettings settings(triangles);
        settings.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;
        return createShape(settings, error);
    }

    void removeTerrainBody(entt::registry& reg, const TileKey& key)
    {
        auto iter = terrainBodies.find(key);
        if (iter == terrainBodies.end())
            return;

        destroyBody(iter->second.bodyID);
        if (iter->second.debugEntity != entt::null && reg.valid(iter->second.debugEntity))
            reg.destroy(iter->second.debugEntity);

        terrainBodies.erase(iter);
    }

    void upsertTerrainBody(entt::registry& reg, TerrainTileNode& tile)
    {
        if (!settings.terrainColliders || !island.valid)
            return;

        auto shape = createTerrainShape(tile);
        if (!shape)
            return;

        auto& terrainBody = terrainBodies[tile.key];
        terrainBody.tile = &tile;
        destroyBody(terrainBody.bodyID);

        JPH::BodyCreationSettings bodySettings(
            shape,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            Layers::NON_MOVING);

        bodySettings.mFriction = 0.8f;
        bodySettings.mRestitution = 0.0f;
        terrainBody.bodyID = bodies().CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);

        if (settings.debugDrawColliders)
        {
            MeshGeometry geom = createTerrainDebugGeometry(tile);
            if (!geom.vertices.empty() && !geom.indices.empty())
                terrainBody.debugEntity = createOrUpdateDebugMesh(reg, terrainBody.debugEntity, geom, nullptr);
        }
        else if (terrainBody.debugEntity != entt::null && reg.valid(terrainBody.debugEntity))
        {
            reg.destroy(terrainBody.debugEntity);
            terrainBody.debugEntity = entt::null;
        }
    }

    bool terrainTileHasResidentDescendant(const TileKey& key)
    {
        for (auto& [candidateKey, weakTile] : terrainResidentTiles)
        {
            if (candidateKey.level <= key.level)
                continue;

            if (!weakTile.ref_ptr())
                continue;

            if (candidateKey.createAncestorKey(key.level) == key)
                return true;
        }

        return false;
    }

    void syncTerrainBodies(entt::registry& reg, const std::set<TileKey>& updatedKeys)
    {
        std::vector<TileKey> staleKeys;
        for (auto& [key, weakTile] : terrainResidentTiles)
        {
            if (!weakTile.ref_ptr())
                staleKeys.emplace_back(key);
        }

        for (auto& key : staleKeys)
        {
            terrainResidentTiles.erase(key);
            removeTerrainBody(reg, key);
        }

        std::set<TileKey> desiredKeys;
        for (auto& [key, weakTile] : terrainResidentTiles)
        {
            if (!weakTile.ref_ptr())
                continue;

            if (!settings.terrainCollidersUseLeafTiles || !terrainTileHasResidentDescendant(key))
                desiredKeys.insert(key);
        }

        for (auto iter = terrainBodies.begin(); iter != terrainBodies.end(); )
        {
            auto key = iter->first;
            ++iter;

            if (desiredKeys.count(key) == 0)
                removeTerrainBody(reg, key);
        }

        for (auto& key : desiredKeys)
        {
            bool needsUpdate =
                terrainBodies.find(key) == terrainBodies.end() ||
                updatedKeys.count(key) != 0;

            if (!needsUpdate)
                continue;

            auto iter = terrainResidentTiles.find(key);
            if (iter != terrainResidentTiles.end())
                if (auto tile = iter->second.ref_ptr())
                    upsertTerrainBody(reg, *tile);
        }
    }

    void processTerrainQueues(entt::registry& reg)
    {
        std::vector<vsg::observer_ptr<TerrainTileNode>> upserts;
        std::vector<TileKey> removes;
        {
            std::scoped_lock lock(terrainQueueMutex);
            upserts.swap(terrainUpserts);
            removes.swap(terrainRemoves);
        }

        std::set<TileKey> updatedKeys;

        for (auto& key : removes)
        {
            terrainResidentTiles.erase(key);
            removeTerrainBody(reg, key);
        }

        for (auto& weakTile : upserts)
        {
            if (auto tile = weakTile.ref_ptr())
            {
                terrainResidentTiles[tile->key] = tile;
                updatedKeys.insert(tile->key);
            }
        }

        if (!settings.terrainColliders)
        {
            for (auto iter = terrainBodies.begin(); iter != terrainBodies.end(); )
            {
                auto key = iter->first;
                ++iter;
                removeTerrainBody(reg, key);
            }
            return;
        }

        syncTerrainBodies(reg, updatedKeys);
    }

    void refreshTerrainDebug(entt::registry& reg)
    {
        for (auto& [key, terrainBody] : terrainBodies)
        {
            if (!settings.debugDrawColliders)
            {
                if (terrainBody.debugEntity != entt::null && reg.valid(terrainBody.debugEntity))
                    reg.destroy(terrainBody.debugEntity);
                terrainBody.debugEntity = entt::null;
                continue;
            }

            if (auto tile = terrainBody.tile.ref_ptr())
            {
                MeshGeometry geom = createTerrainDebugGeometry(*tile);
                if (!geom.vertices.empty() && !geom.indices.empty())
                    terrainBody.debugEntity = createOrUpdateDebugMesh(reg, terrainBody.debugEntity, geom, nullptr);
            }
        }
    }

    PhysicsSettings getOrCreateSettings(entt::registry& reg)
    {
        PhysicsSettings out;
        auto view = reg.view<PhysicsSettings>();
        if (view.begin() == view.end())
        {
            settingsEntity = reg.create();
            auto& settingsComponent = reg.emplace<PhysicsSettings>(settingsEntity);
            settingsComponent.owner = settingsEntity;
            out = settingsComponent;
        }
        else
        {
            settingsEntity = *view.begin();
            out = reg.get<PhysicsSettings>(settingsEntity);
        }
        return out;
    }

    void step(VSGContext context, entt::registry& reg)
    {
        auto frameStamp = context->viewer()->getFrameStamp();
        if (!frameStamp)
            return;

        auto time = frameStamp->time;
        if (lastUpdateTime != vsg::time_point::min())
        {
            double dt = std::chrono::duration<double>(time - lastUpdateTime).count();
            if (dt > 0.0 && settings.fixedTimeStep > 0.0)
            {
                const double fixedTimeStep = settings.fixedTimeStep;
                const int maxSubSteps = std::max(1, settings.maxSubSteps);

                accumulator += dt;
                int steps = 0;
                while (accumulator >= fixedTimeStep && steps < maxSubSteps)
                {
                    physics->SetGravity(toJoltVec3(settings.gravity));
                    physics->Update((float)fixedTimeStep, 1, tempAllocator.get(), jobSystem.get());
                    accumulator -= fixedTimeStep;
                    ++steps;
                }

                if (steps == maxSubSteps && accumulator >= fixedTimeStep)
                    accumulator = std::fmod(accumulator, fixedTimeStep);

                if (steps > 0)
                {
                    reg.view<RigidBody, Transform, JoltRigidBodyDetail>().each(
                        [&](RigidBody& body, Transform& transform, JoltRigidBodyDetail& detail)
                        {
                            syncTransformFromBody(reg, body, transform, detail);
                        });

                    context->requestFrame();
                }
            }
        }

        lastUpdateTime = time;
    }
};

JoltPhysicsSystem::JoltPhysicsSystem(Registry& registry, vsg::ref_ptr<TerrainNode> terrain) :
    System(registry),
    _impl(std::make_unique<Impl>(registry))
{
    registry.write([&](entt::registry& r)
        {
            r.on_construct<RigidBody>().connect<&on_construct_RigidBody>();
            r.on_update<RigidBody>().connect<&on_update_RigidBody>();
            r.on_destroy<RigidBody>().connect<&on_destroy_RigidBody>();

            r.on_construct<BoxCollider>().connect<&on_construct_BoxCollider>();
            r.on_construct<SphereCollider>().connect<&on_construct_SphereCollider>();
            r.on_construct<CapsuleCollider>().connect<&on_construct_CapsuleCollider>();
            r.on_construct<CylinderCollider>().connect<&on_construct_CylinderCollider>();
            r.on_construct<MeshCollider>().connect<&on_construct_MeshCollider>();
            r.on_construct<ConvexHullCollider>().connect<&on_construct_ConvexHullCollider>();

            r.on_update<BoxCollider>().connect<&on_update_BoxCollider>();
            r.on_update<SphereCollider>().connect<&on_update_SphereCollider>();
            r.on_update<CapsuleCollider>().connect<&on_update_CapsuleCollider>();
            r.on_update<CylinderCollider>().connect<&on_update_CylinderCollider>();
            r.on_update<MeshCollider>().connect<&on_update_MeshCollider>();
            r.on_update<ConvexHullCollider>().connect<&on_update_ConvexHullCollider>();

            r.on_destroy<BoxCollider>().connect<&on_destroy_BoxCollider>();
            r.on_destroy<SphereCollider>().connect<&on_destroy_SphereCollider>();
            r.on_destroy<CapsuleCollider>().connect<&on_destroy_CapsuleCollider>();
            r.on_destroy<CylinderCollider>().connect<&on_destroy_CylinderCollider>();
            r.on_destroy<MeshCollider>().connect<&on_destroy_MeshCollider>();
            r.on_destroy<ConvexHullCollider>().connect<&on_destroy_ConvexHullCollider>();

            auto dirty = r.create();
            r.emplace<RigidBody::Dirty>(dirty);
            r.emplace<BoxCollider::Dirty>(dirty);
            r.emplace<SphereCollider::Dirty>(dirty);
            r.emplace<CapsuleCollider::Dirty>(dirty);
            r.emplace<CylinderCollider::Dirty>(dirty);
            r.emplace<MeshCollider::Dirty>(dirty);
            r.emplace<ConvexHullCollider::Dirty>(dirty);
            r.emplace<JoltDestructionQueue>(dirty);
        });

    setTerrainNode(terrain);
}

JoltPhysicsSystem::~JoltPhysicsSystem()
{
    auto write = _registry.write();
    _impl->shutdownPhysics(&write.registry);
}

void
JoltPhysicsSystem::initialize(VSGContext)
{
    auto [lock, reg] = _registry.write();
    _impl->settings = _impl->getOrCreateSettings(reg);
    _impl->initializePhysics(_impl->settings);
}

void
JoltPhysicsSystem::update(VSGContext context)
{
    if (status.failed())
        return;

    auto [lock, reg] = _registry.write();

    _impl->settings = _impl->getOrCreateSettings(reg);
    if (!_impl->settings.enabled)
        return;

    _impl->initializePhysics(_impl->settings);
    _impl->drainDestructionQueue(reg);
    _impl->processDirtyLists(reg);

    if (!_impl->ensureIsland(reg))
        return;

    if (_impl->shouldRebase(reg))
        _impl->rebaseIsland(reg);

    _impl->rebuildBodiesIfNeeded(reg);
    _impl->processTerrainQueues(reg);
    _impl->refreshTerrainDebug(reg);
    _impl->step(context, reg);
}

void
JoltPhysicsSystem::setTerrainNode(vsg::ref_ptr<TerrainNode> terrain)
{
    _impl->terrainSubscriptions.clear();
    _impl->terrain = terrain;

    if (!terrain)
        return;

    _impl->terrainSubscriptions += terrain->onTileResident([this](TerrainTileNode* tile)
        {
            std::scoped_lock lock(_impl->terrainQueueMutex);
            if (tile)
                _impl->terrainUpserts.emplace_back(tile);
        });

    _impl->terrainSubscriptions += terrain->onTileReleased([this](TerrainTileNode* tile)
        {
            if (!tile)
                return;
            std::scoped_lock lock(_impl->terrainQueueMutex);
            _impl->terrainRemoves.emplace_back(tile->key);
        });
}

std::size_t
JoltPhysicsSystem::numBodies() const
{
    return _impl && _impl->physics ? _impl->physics->GetNumBodies() : 0u;
}

#else // ROCKY_HAS_JOLT

struct JoltPhysicsSystem::Impl
{
    Impl(Registry) { }
};

JoltPhysicsSystem::JoltPhysicsSystem(Registry& registry, vsg::ref_ptr<TerrainNode>) :
    System(registry),
    _impl(std::make_unique<Impl>(registry))
{
    status = Failure(Failure::ResourceUnavailable, "Rocky was built without Jolt Physics support.");
}

JoltPhysicsSystem::~JoltPhysicsSystem() = default;
void JoltPhysicsSystem::initialize(VSGContext) { }
void JoltPhysicsSystem::update(VSGContext) { }
void JoltPhysicsSystem::setTerrainNode(vsg::ref_ptr<TerrainNode>) { }
std::size_t JoltPhysicsSystem::numBodies() const { return 0u; }

#endif
