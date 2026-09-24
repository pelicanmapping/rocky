/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/vsg/ecs/OpticsSystem.h>
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/vsg/terrain/TerrainNode.h>
#include <rocky/vsg/terrain/GeometryPool.h>
#include <rocky/vsg/VSGUtils.h>
#include <limits>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    //! Headless resident-terrain fixture; geometry and tile bounds use the real surface implementation.
    struct DepthFixture
    {
        VSGContextSingleton context = VSGContextFactory::create(vsg::Viewer::create());
        Registry registry = Registry::create();
        vsg::ref_ptr<OpticsSystemNode> optics = OpticsSystemNode::create(registry);
        vsg::ref_ptr<TerrainNode> terrain = TerrainNode::create(context.get());
        Profile profile;
        vsg::ref_ptr<TerrainProfileNode> profileNode;
        vsg::ref_ptr<TerrainTileNode> root;
        entt::entity entity = entt::null;

        //! Creates a small terrain profile in meters or degrees, avoiding paging, network access, and Vulkan objects.
        explicit DepthFixture(bool globe = false)
        {
            REQUIRE(terrain->status.ok());
            terrain->renderingSRS = globe ? SRS::ECEF : SRS::SPHERICAL_MERCATOR;
            profile = globe ? Profile(SRS::WGS84, Box(-0.15, 51.48, -0.10, 51.52), 1u, 1u) :
                Profile(SRS::SPHERICAL_MERCATOR, Box(-2000.0, -2000.0, 2000.0, 2000.0), 1u, 1u);
            terrain->profile = profile;
            profileNode = TerrainProfileNode::create(profile, *terrain, context.get());
            auto state = terrain->children.front().node.cast<vsg::Group>();
            REQUIRE(state);
            state->addChild(profileNode);
            root = makeTile(TileKey(0u, 0u, 0u, profile), 40.0);
            profileNode->addChild(root);
            optics->target = terrain;
        }

        //! Builds an elevated quad with a double-precision tile frame and the same proxy/bounds path as real terrain.
        vsg::ref_ptr<TerrainTileNode> makeTile(const TileKey& key, double height)
        {
            auto tile = TerrainTileNode::create();
            tile->key = key;
            tile->surface = SurfaceNode::create(key, terrain->renderingSRS);
            auto geometry = SharedGeometry::create();
            geometry->verts = vsg::vec3Array::create(4);
            geometry->normals = vsg::vec3Array::create(4);
            geometry->uvs = vsg::vec3Array::create(4);
            geometry->indexArray = vsg::ushortArray::create({ 0, 1, 2, 0, 2, 3 });
            const auto extent = key.extent();
            const auto inverse = glm::inverse(to_glm(tile->surface->matrix));
            const glm::dvec2 corners[] = {
                { extent.xmin(), extent.ymin() }, { extent.xmax(), extent.ymin() },
                { extent.xmax(), extent.ymax() }, { extent.xmin(), extent.ymax() } };
            for (unsigned i = 0u; i < 4u; ++i)
            {
                const auto world = GeoPoint(profile.srs(), corners[i].x, corners[i].y, height)
                    .transform(terrain->renderingSRS);
                geometry->verts->at(i) = to_vsg(glm::vec3(inverse * glm::dvec4(world.x, world.y, world.z, 1.0)));
            }
            geometry->assignArrays({ geometry->verts });
            geometry->assignIndices(geometry->indexArray);
            geometry->commands.emplace_back(vsg::DrawIndexed::create(6, 1, 0, 0, 0));
            auto group = vsg::Group::create();
            group->addChild(geometry);
            tile->surface->addChild(group);
            tile->surface->setElevation({}, glm::mat4(1.0));
            tile->bound = tile->surface->worldBoundingSphere;
            tile->addChild(tile->surface);
            return tile;
        }

        //! Seeds the normal per-view projection inputs with elevated source geometry; no atlas or bake is needed.
        void addOverlay(OverlayMode mode, bool automatic = true)
        {
            registry.write([&](entt::registry& reg)
            {
                entity = reg.create();
                reg.emplace<Overlay>(entity).mode = mode;
                if (automatic)
                    reg.emplace<AutoOverlayTransform>(entity);
                reg.emplace<ProjectedTexture>(entity).placement = ProjectionPlacement::Terrain;
                auto& view = reg.emplace<TransformDetail>(entity).views[0];
                view.revision = 0;
                view.cache.world_srs = terrain->renderingSRS;
                auto center = profile.extent().centroid();
                center.z = 1800.0;
                const auto frame = terrain->renderingSRS.topocentricToWorldMatrix(center.transform(terrain->renderingSRS));
                view.model = to_vsg(frame * glm::scale(glm::dmat4(1.0), glm::dvec3(800.0, 600.0, 1000.0)));
            });
        }

        //! Copies cached results while holding the registry read lock.
        ProjectionViewDetail depth() const
        {
            auto reader = registry.read();
            return reader.registry.get<ProjectionDetail>(entity).views[0];
        }
    };
}

//! Flat/globe overlays use a local receiving range without altering XY mapping, regardless of payload technique.
TEST_CASE("automatic overlay depth follows loaded terrain without moving artwork", "[projection][overlay-depth]")
{
    const bool globe = GENERATE(Catch::values(false, true));
    const auto mode = GENERATE(Catch::values(OverlayMode::Raster, OverlayMode::Vector));
    DepthFixture f(globe);
    f.addOverlay(mode);
    f.optics->update(f.context.get());
    const auto detail = f.depth();
    REQUIRE(detail.terrainDepthRangeValid);
    auto fitted = detail.depthProjectorWorld;
    REQUIRE(detail.applyTerrainDepth(fitted));
    const double thickness = glm::length(glm::dvec3(fitted[2]));
    CHECK(thickness >= 49.99);
    CHECK(thickness < 100.0);
    const auto oldInverse = glm::inverse(detail.depthProjectorWorld);
    const auto newInverse = glm::inverse(fitted);
    for (const glm::dvec3 local : { glm::dvec3(-0.4, 0.3, -0.1), glm::dvec3(0.2, -0.4, 0.2) })
    {
        const auto point = detail.depthProjectorWorld * glm::dvec4(local, 1.0);
        CHECK(glm::length(glm::dvec2(oldInverse * point) - glm::dvec2(newInverse * point)) < 1e-8);
    }
    auto moved = detail.depthProjectorWorld;
    moved[3].x += 1.0;
    CHECK_FALSE(detail.applyTerrainDepth(moved));
}

//! Cache invalidation uses the entire footprint, not just the center, and caches a safe fallback on coverage loss.
TEST_CASE("overlay depth refreshes on local bounds changes and resets", "[projection][overlay-depth]")
{
    DepthFixture f;
    f.addOverlay(OverlayMode::Vector);
    f.optics->update(f.context.get());
    const auto original = f.depth();
    REQUIRE(original.terrainDepthRangeValid);
    f.root->surface->localbbox.max.z += 150.0;
    f.optics->update(f.context.get());
    CHECK(f.depth().terrainDepthRange == original.terrainDepthRange);

    const TileKey unrelated(0u, 0u, 0u,
        Profile(SRS::SPHERICAL_MERCATOR, Box(10000.0, 10000.0, 11000.0, 11000.0), 1u, 1u));
    f.terrain->onTileBoundsChanged.fire(unrelated);
    f.optics->update(f.context.get());
    CHECK(f.depth().terrainDepthRange == original.terrainDepthRange);

    // This small area overlaps the footprint's edge but does not contain the center-ray hit.
    const TileKey edge(0u, 0u, 0u,
        Profile(SRS::SPHERICAL_MERCATOR, Box(300.0, 100.0, 450.0, 200.0), 1u, 1u));
    f.terrain->onTileBoundsChanged.fire(edge);
    f.optics->update(f.context.get());
    REQUIRE(f.depth().terrainDepthRangeValid);
    CHECK(f.depth().terrainDepthRange.y == Approx(original.terrainDepthRange.y + 0.15));

    f.root->surface->localbbox = {};
    f.terrain->onTileBoundsChanged.fire(TileKey{});
    f.optics->update(f.context.get());
    CHECK_FALSE(f.depth().terrainDepthRangeValid);
    CHECK(f.depth().terrainDepthCacheValid);
    auto unchanged = f.depth().depthProjectorWorld;
    CHECK_FALSE(f.depth().applyTerrainDepth(unchanged));
}

//! Finest resident children replace broad ancestor boxes; missing coverage and bad transforms retain the fallback.
TEST_CASE("terrain height ranges use complete resident coverage", "[projection][overlay-depth]")
{
    DepthFixture f;
    const GeoExtent footprint(SRS::SPHERICAL_MERCATOR, -500.0, -500.0, 500.0, 500.0);
    auto range = f.terrain->localHeightRange(footprint, glm::dmat4(1.0));
    REQUIRE(range.ok());
    CHECK(range.value().x == Approx(40.0));
    auto quad = vsg::QuadGroup::create();
    for (unsigned i = 0; i < 4u; ++i)
        quad->children[i] = f.makeTile(f.root->key.createChildKey(i), 100.0 + 10.0 * i);
    f.root->addChild(quad);
    range = f.terrain->localHeightRange(footprint, glm::dmat4(1.0));
    REQUIRE(range.ok());
    CHECK(range.value().x == Approx(100.0));
    CHECK(range.value().y == Approx(130.0));

    SECTION("page out children")
    {
        f.root->children.resize(1u);
        range = f.terrain->localHeightRange(footprint, glm::dmat4(1.0));
        REQUIRE(range.ok());
        CHECK(range.value().y == Approx(40.0));
    }
    SECTION("incomplete child set")
    {
        quad->children[0] = {};
        CHECK(f.terrain->localHeightRange(footprint, glm::dmat4(1.0)).failed());
    }
    SECTION("incomplete profile coverage")
    {
        CHECK(f.terrain->localHeightRange(
            GeoExtent(SRS::SPHERICAL_MERCATOR, -2500.0, -500.0, 500.0, 500.0), glm::dmat4(1.0)).failed());
    }
    SECTION("invalid transform")
    {
        auto invalid = glm::dmat4(1.0);
        invalid[0][0] = std::numeric_limits<double>::quiet_NaN();
        CHECK(f.terrain->localHeightRange(footprint, invalid).failed());
    }
}

//! Explicit projectors and fixed placement must retain their existing depth even when terrain bounds are available.
TEST_CASE("explicit overlay volumes are not automatically tightened", "[projection][overlay-depth]")
{
    DepthFixture f;
    bool automatic = false;
    SECTION("explicit transform") { }
    SECTION("fixed placement") { automatic = true; }
    f.addOverlay(OverlayMode::Vector, automatic);
    if (automatic)
        f.registry.write([&](entt::registry& reg)
        {
            reg.get<ProjectedTexture>(f.entity).placement = ProjectionPlacement::Fixed;
        });
    f.optics->update(f.context.get());
    CHECK_FALSE(f.depth().terrainDepthRangeValid);
}

//! A footprint crossing the date line must collect both sides without expanding into a world-wide range.
TEST_CASE("terrain height ranges cross the antimeridian", "[projection][overlay-depth]")
{
    DepthFixture f(true);
    f.profile = Profile("global-geodetic");
    f.terrain->profile = f.profile;
    f.profileNode->profile = f.profile;
    f.profileNode->children.clear();
    for (double longitude : { 179.9995, -179.9995 })
    {
        const auto key = TileKey::createTileKeyContainingPoint(GeoPoint(SRS::WGS84, longitude, 35.0, 0.0),
            12u, f.profile);
        f.profileNode->addChild(f.makeTile(key, 40.0));
    }
    const auto origin = GeoPoint(SRS::WGS84, 180.0, 35.0, 0.0).transform(SRS::ECEF);
    const auto inverse = glm::inverse(SRS::ECEF.topocentricToWorldMatrix(origin));
    const auto result = f.terrain->localHeightRange(
        GeoExtent(SRS::WGS84, 179.999, 34.999, -179.999, 35.001), inverse);
    REQUIRE(result.ok());
    CHECK(result.value().x > 30.0);
    CHECK(result.value().y < 50.0);
}
