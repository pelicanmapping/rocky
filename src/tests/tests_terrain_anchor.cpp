/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/ecs/TerrainAnchorSystem.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <rocky/vsg/VSGUtils.h>
#include <vsg/utils/LineSegmentIntersector.h>
#include <cmath>
#include <limits>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Counts real intersection traversals without adding a query hook to the production API.
    struct QueryCounter : vsg::Inherit<vsg::Group, QueryCounter>
    {
        mutable unsigned queries = 0u;

        //! Counts intersection visitors only; normal scene traversal remains unchanged.
        void traverse(vsg::ConstVisitor& visitor) const override
        {
            if (dynamic_cast<vsg::LineSegmentIntersector*>(&visitor))
                ++queries;
            vsg::Group::traverse(visitor);
        }
    };

    //! Headless terrain/anchor fixture using real double-precision transforms and local float triangle vertices.
    struct AnchorFixture
    {
        VSGContextSingleton context = VSGContextFactory::create(vsg::Viewer::create());
        Registry registry = Registry::create();
        TerrainAnchorSystem system{ registry };
        vsg::ref_ptr<TerrainNode> terrain;
        vsg::ref_ptr<QueryCounter> counter = QueryCounter::create();
        const GeoPoint origin{ SRS::WGS84, -0.1278, 51.5, 0.0 };
        entt::entity entity = entt::null;

        //! Builds terrain state without a Vulkan device, replacing its tiles with a small patch near London.
        explicit AnchorFixture(const SRS& renderingSRS)
        {
            terrain = TerrainNode::create(context.get());
            REQUIRE(terrain->status.ok());
            terrain->renderingSRS = renderingSRS;
            terrain->children.clear();
            terrain->addChild(vsg::MASK_ALL, counter);
            system.target = terrain;
            setHeight(40.0);
        }

        //! Replaces loaded geometry without firing a load event, so tests control cache invalidation explicitly.
        void setHeight(double height)
        {
            auto center = origin;
            center.z = height;
            center = center.transform(terrain->renderingSRS);
            REQUIRE(center.valid());
            auto patch = vsg::MatrixTransform::create(
                to_vsg(terrain->renderingSRS.topocentricToWorldMatrix(center)));
            auto vertices = vsg::vec3Array::create(4);
            vertices->at(0).set(-1000.0f, -1000.0f, 0.0f);
            vertices->at(1).set(1000.0f, -1000.0f, 0.0f);
            vertices->at(2).set(1000.0f, 1000.0f, 0.0f);
            vertices->at(3).set(-1000.0f, 1000.0f, 0.0f);
            auto indices = vsg::ushortArray::create({ 0, 1, 2, 0, 2, 3 });
            auto geometry = vsg::Geometry::create();
            geometry->assignArrays({ vertices });
            geometry->assignIndices(indices);
            geometry->commands.emplace_back(vsg::DrawIndexed::create(6, 1, 0, 0, 0));
            patch->addChild(geometry);
            counter->children.clear();
            counter->addChild(patch);
        }

        //! Adds a user-authored Transform and its optional-height-offset anchor under the registry lock.
        void addAnchor(const GeoPoint& position, double offset)
        {
            registry.write([&](entt::registry& reg)
            {
                entity = reg.create();
                reg.emplace<Transform>(entity).position = position;
                reg.emplace<TerrainAnchor>(entity).offset = offset;
            });
        }

        //! Runs one anchor update with no graphics device or terrain paging involved.
        void update()
        {
            system.update(context.get());
        }

        //! Returns a copy of the current position without leaking a registry reference.
        GeoPoint position()
        {
            GeoPoint result;
            registry.read([&](entt::registry& reg) { result = reg.get<Transform>(entity).position; });
            return result;
        }

        //! Checks horizontal invariance and the expected ellipsoid height, allowing sub-millimeter conversion error.
        void checkPosition(double height)
        {
            auto point = position().transform(SRS::WGS84);
            REQUIRE(point.valid());
            CHECK(std::abs(point.x - origin.x) < 1e-9);
            CHECK(std::abs(point.y - origin.y) < 1e-9);
            CHECK(std::abs(point.z - height) < 1e-4);
        }

        //! Simulates a completed terrain load in either the western root (contains London) or eastern root.
        void loadTile(bool relevant)
        {
            terrain->onTileLoaded.fire(TileKey(0, relevant ? 0 : 1, 0, Profile("global-geodetic")));
        }
    };
}

//! Demonstrates the radial/vertical distinction on WGS84 and checks that input altitude cannot move the query.
TEST_CASE("vertical terrain queries preserve geodetic location", "[ecs][terrain-anchor]")
{
    AnchorFixture f(SRS::ECEF);
    for (double height : { -500.0, 0.0, 1800.0, 50000.0 })
    {
        auto input = f.origin;
        input.z = height;
        auto result = f.terrain->intersectVertical(input);
        REQUIRE(result.ok());
        auto point = result.value().point.transform(SRS::WGS84);
        CHECK(std::abs(point.x - input.x) < 1e-9);
        CHECK(std::abs(point.y - input.y) < 1e-9);
        CHECK(std::abs(point.z - 40.0) < 1e-4);
        const auto expectedUp = glm::dvec3(SRS::ECEF.topocentricToWorldMatrix(result.value().point)[2]);
        CHECK(glm::dot(expectedUp, glm::dvec3(result.value().normal)) > 0.999999);
    }

    auto elevated = f.origin;
    elevated.z = 1800.0;
    auto radial = f.terrain->intersect(elevated);
    auto vertical = f.terrain->intersectVertical(elevated);
    REQUIRE(radial.ok());
    REQUIRE(vertical.ok());
    // Preserve the old radial API, while proving that using it for anchors moves the hit by several meters.
    CHECK(glm::length(glm::dvec3(radial.value().point) - glm::dvec3(vertical.value().point)) > 1.0);
}

//! Covers projected XY rays, failed queries, and rejection of non-finite input before scene traversal.
TEST_CASE("vertical terrain queries support projected maps and safe misses", "[ecs][terrain-anchor]")
{
    AnchorFixture f(SRS::SPHERICAL_MERCATOR);
    auto input = f.origin.transform(SRS::SPHERICAL_MERCATOR);
    input.z = 1800.0;
    auto result = f.terrain->intersectVertical(input);
    REQUIRE(result.ok());
    CHECK(std::abs(result.value().point.x - input.x) < 1e-7);
    CHECK(std::abs(result.value().point.y - input.y) < 1e-7);
    CHECK(std::abs(result.value().point.z - 40.0) < 1e-7);

    input.x += 5000.0;
    CHECK(f.terrain->intersectVertical(input).failed());
    const auto queries = f.counter->queries;
    input.x = std::numeric_limits<double>::quiet_NaN();
    CHECK(f.terrain->intersectVertical(input).failed());
    CHECK(f.terrain->intersectVertical(GeoPoint{}).failed());
    CHECK(f.counter->queries == queries);
}

//! Exercises repeated updates and offset/altitude edits across geographic, ECEF, and projected input coordinates.
TEST_CASE("terrain anchors preserve position and reuse terrain heights", "[ecs][terrain-anchor]")
{
    const SRS worldSRS = GENERATE(Catch::values(SRS::ECEF, SRS::SPHERICAL_MERCATOR));
    const SRS inputSRS = GENERATE(Catch::values(SRS::WGS84, SRS::ECEF, SRS::SPHERICAL_MERCATOR));
    const double offset = GENERATE(Catch::values(0.0, 10.0, -5.0));
    AnchorFixture f(worldSRS);
    auto elevated = f.origin;
    elevated.z = 1800.0;
    const auto input = elevated.transform(inputSRS);
    f.addAnchor(input, offset);
    f.update();
    f.checkPosition(40.0 + offset);
    CHECK(f.counter->queries == 1u);
    const auto settled = f.position();
    if (!inputSRS.isGeocentric())
    {
        CHECK(settled.x == input.x);
        CHECK(settled.y == input.y);
    }

    for (unsigned frame = 0; frame < 1000; ++frame)
        f.update();
    CHECK(f.position() == settled);
    CHECK(f.counter->queries == 1u);
    f.registry.read([&](entt::registry& reg) { CHECK(reg.get<Transform>(f.entity).revision == 1); });

    f.registry.write([&](entt::registry& reg) { reg.get<TerrainAnchor>(f.entity).offset = offset + 15.0; });
    f.update();
    f.checkPosition(55.0 + offset);
    CHECK(f.counter->queries == 1u);

    // An application can edit altitude, but the anchor still owns it; this does not require another terrain query.
    f.registry.write([&](entt::registry& reg) { reg.get<Transform>(f.entity).position = input; });
    f.update();
    f.checkPosition(55.0 + offset);
    CHECK(f.counter->queries == 1u);

    // A genuine horizontal edit must invalidate the cache and become the new authoritative location.
    elevated.x += 0.0001;
    f.registry.write([&](entt::registry& reg)
    {
        reg.get<Transform>(f.entity).position = elevated.transform(inputSRS);
    });
    f.update();
    CHECK(f.counter->queries == 2u);
    CHECK(std::abs(f.position().transform(SRS::WGS84).x - elevated.x) < 1e-9);
}

//! Verifies tile, target, and rendering-SRS invalidation without changing the anchor's horizontal reference.
TEST_CASE("terrain anchor height caches respond to terrain changes", "[ecs][terrain-anchor]")
{
    const SRS worldSRS = GENERATE(Catch::values(SRS::ECEF, SRS::SPHERICAL_MERCATOR));
    AnchorFixture f(worldSRS);
    auto input = f.origin;
    input.z = 1800.0;
    f.addAnchor(input.transform(SRS::ECEF), 10.0);
    f.update();
    const auto settled = f.position();
    f.setHeight(100.0);
    f.loadTile(false);
    f.update();
    CHECK(f.position() == settled);
    CHECK(f.counter->queries == 1u);

    for (unsigned load = 0; load < 20; ++load)
    {
        f.setHeight(100.0 + load);
        f.loadTile(true);
        f.update();
        f.checkPosition(110.0 + load);
        f.update();
        CHECK(f.counter->queries == 2u + load);
    }

    auto replacement = TerrainNode::create(f.context.get());
    replacement->renderingSRS = worldSRS;
    replacement->children.clear();
    replacement->addChild(vsg::MASK_ALL, f.counter);
    f.system.target = replacement;
    f.update();
    CHECK(f.counter->queries == 22u);
    f.checkPosition(129.0);

    f.terrain = replacement;
    f.terrain->renderingSRS = worldSRS.isGeocentric() ? SRS::SPHERICAL_MERCATOR : SRS::ECEF;
    f.setHeight(200.0);
    f.update();
    CHECK(f.counter->queries == 23u);
    f.checkPosition(210.0);
}

//! Ensures missing terrain and invalid offsets leave user coordinates intact, with cached misses retried on load.
TEST_CASE("terrain anchors leave unresolved positions unchanged", "[ecs][terrain-anchor]")
{
    AnchorFixture f(SRS::ECEF);
    f.counter->children.clear();
    auto input = f.origin;
    input.z = 1800.0;
    f.addAnchor(input, 10.0);
    for (unsigned frame = 0; frame < 10; ++frame)
        f.update();
    CHECK(f.counter->queries == 1u);
    CHECK(f.position() == input);
    f.setHeight(40.0);
    f.loadTile(true);
    f.update();
    f.checkPosition(50.0);
    CHECK(f.counter->queries == 2u);

    const auto settled = f.position();
    f.registry.write([&](entt::registry& reg)
    {
        reg.get<TerrainAnchor>(f.entity).offset = std::numeric_limits<double>::infinity();
    });
    f.update();
    CHECK(f.position() == settled);
    CHECK(f.counter->queries == 2u);

    f.registry.write([&](entt::registry& reg) { reg.remove<TerrainAnchor>(f.entity); });
    f.loadTile(true);
    f.update();
    CHECK(f.position() == settled);
    CHECK(f.counter->queries == 2u);
}
