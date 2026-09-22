/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/Common.h>

#ifdef ROCKY_HAS_SLUGHORN
#include <rocky/ecs/Line.h>
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Point.h>
#include <rocky/ecs/Polygon.h>
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#include <rocky/vsg/ecs/OverlayRenderContext.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include <rocky/vsg/ecs/PolygonSystem.h>
#include <rocky/vsg/ecs/SlugResource.h>
#include <rocky/vsg/ecs/SlugSystem.h>
#include "../rocky/vsg/ecs/slug/SlugAdapter.h"
#include <rocky/Log.h>
#include <spdlog/sinks/ostream_sink.h>
#include <limits>
#include <sstream>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    //! Makes one valid, indivisible rectangle containing separated tall holes.
    //! Long vertical edges deliberately stress repeated band references.
    PolygonPart slottedPolygon(unsigned holes)
    {
        PolygonPart part;
        part.outer = { { -0.4, -0.4, 0.0 }, { 0.4, -0.4, 0.0 },
                       { 0.4, 0.4, 0.0 }, { -0.4, 0.4, 0.0 } };
        for (unsigned i = 0u; i < holes; ++i)
        {
            const double step = 0.7 / holes;
            const double x = -0.35 + i * step;
            part.holes.push_back({ { x, -0.35, 0.0 }, { x, 0.35, 0.0 },
                { x + step * 0.5, 0.35, 0.0 }, { x + step * 0.5, -0.35, 0.0 } });
        }
        return part;
    }

    //! Adds a polygon and a dense boundary-line payload in Geocoder's authoring
    //! order. Tall, disjoint segments stress the SDK's generated stroke curves,
    //! including round caps, rather than the polygon-only preflight.
    entt::entity denseBoundaryOverlay(entt::registry& reg, unsigned segments, float outline, float alpha)
    {
        const auto entity = reg.create();
        auto& polygon = reg.emplace<PolygonGeometry>(entity);
        polygon.polygons.push_back(slottedPolygon(0u));
        reg.emplace<rocky::Polygon>(entity, polygon);
        auto& geometry = reg.emplace<LineGeometry>(entity);
        geometry.topology = LineTopology::Segments;
        for (unsigned i = 0u; i < segments; ++i)
        {
            const double x = -0.35 + 0.7 * i / segments;
            geometry.points.emplace_back(x, -0.4, 0.0);
            geometry.points.emplace_back(x, 0.4, 0.0);
        }
        auto& style = reg.emplace<LineStyle>(entity);
        style.width = 0.1f;
        style.outlineWidth = outline;
        style.color.a = alpha;
        reg.emplace<Line>(entity, geometry, style);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
        return entity;
    }

    //! Captures info/warning messages for repeated-frame tests and restores the process
    //! logger even when a REQUIRE aborts the test. Used only on the test thread.
    struct LogCapture
    {
        std::ostringstream text;
        Logger logger = Log();
        std::vector<log::sink_ptr> savedSinks = logger->sinks();
        log::level::level_enum savedLevel = logger->level();

        //! Redirects info and higher-severity messages to the test-owned stream.
        LogCapture()
        {
            auto sink = std::make_shared<log::sinks::ostream_sink_mt>(text);
            sink->set_pattern("%l: %v");
            logger->sinks() = { sink };
            logger->set_level(log::level::info);
        }

        //! Restores logging before releasing the captured stream.
        ~LogCapture()
        {
            logger->sinks() = std::move(savedSinks);
            logger->set_level(savedLevel);
        }
    };
}

//! Covers each texture/axis independently, including the inclusive device
//! boundary and CPU-only operation. Dimension checks need no Vulkan allocation.
TEST_CASE("slug atlas dimensions respect the device image limit", "[projection][slug][atlas-dimensions]")
{
    SlugAtlasOutput atlas;
    atlas.curveTexture.width = atlas.curveTexture.height = 512u;
    atlas.bandTexture.width = atlas.bandTexture.height = 512u;
    std::uint32_t limit = 512u;
    std::string textureName;
    SECTION("both textures exactly at the limit") {}
    SECTION("curve width exceeds limit") { atlas.curveTexture.width = 513u; textureName = "curve"; }
    SECTION("curve height exceeds limit") { atlas.curveTexture.height = 513u; textureName = "curve"; }
    SECTION("band width exceeds limit") { atlas.bandTexture.width = 513u; textureName = "band"; }
    SECTION("band height exceeds limit") { atlas.bandTexture.height = 513u; textureName = "band"; }
    SECTION("CPU-only atlas has no device limit") { limit = 0u; atlas.bandTexture.height = 4096u; }

    std::string error = "previous failure";
    CHECK(checkSlugAtlasDimensions(atlas, limit, error) == textureName.empty());
    if (textureName.empty())
        CHECK(error.empty());
    else
    {
        CHECK(error.find("Slug " + textureName + " atlas dimensions ") != std::string::npos);
        CHECK(error.find("513") != std::string::npos);
        CHECK(error.find("maxImageDimension2D=512") != std::string::npos);
    }
}

//! Exercises the non-polygon capacity path, including both outline encodings;
//! successful reductions must reach the logger once, not once per update.
TEST_CASE("slug boundary lines reduce bands before atlas packing", "[slug][overlay-fallback][slug-boundary]")
{
    unsigned segments = 1024u;
    float outline = 0.0f, alpha = 1.0f;
    SECTION("boundary without outline") {}
    SECTION("opaque outline casing") { outline = 0.1f; }
    SECTION("translucent outline ring") { segments = 512u; outline = 0.1f; alpha = 0.5f; }
    INFO("segments=" << segments << ", outline=" << outline << ", alpha=" << alpha);

    LogCapture messages;
    Registry registry = Registry::create();
    auto slug = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = denseBoundaryOverlay(reg, segments, outline, alpha);
    });
    slug->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        INFO(resource.message);
        REQUIRE(resource.ready);
        REQUIRE(resource.layers.size() == (outline > 0.0f ? 3u : 2u));
        CHECK_FALSE(reg.any_of<OverlayVectorFallback>(entity));
        // Only the line layers need reduced bands; the polygon remains unchanged.
        CHECK(resource.layers.front().shapeData.z == 1u);
        for (std::size_t i = 1u; i < resource.layers.size(); ++i)
        {
            CHECK(resource.layers[i].shapeData.z <= 31u);
            CHECK(resource.layers[i].shapeData.w <= 31u);
            CHECK(resource.layers[i].shapeData.z >= 7u);
            CHECK(resource.layers[i].shapeData.w >= 7u);
        }
    });
    const auto firstMessage = messages.text.str();
    CHECK(firstMessage.find("info: SlugSystemNode: overlay ") != std::string::npos);
    if (segments >= 1024u)
        CHECK(firstMessage.find("shape rocky-overlay-1 reduced bands") != std::string::npos);
    if (outline > 0.0f)
        CHECK(firstMessage.find("shape rocky-overlay-1-outline reduced bands") != std::string::npos);
    for (unsigned frame = 0u; frame < 3u; ++frame)
        slug->update(context.get());
    CHECK(messages.text.str() == firstMessage);
}

//! Boundaries that would need fewer than eight bands route the whole overlay
//! to Raster even when its polygon fits, without retries on unchanged frames.
TEST_CASE("slug boundary exhaustion falls back to raster", "[slug][overlay-fallback][slug-boundary]")
{
    unsigned segments = 6000u;
    float outline = 0.0f, alpha = 1.0f;
    SECTION("previously fit at one band") {}
    SECTION("exceeds even the one-band capacity") { segments = 20000u; }
    SECTION("outline ring requires fewer than eight bands") { segments = 1024u; outline = 0.1f; alpha = 0.5f; }
    LogCapture messages;
    Registry registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    bake->bakeScene = {};
    auto slug = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = denseBoundaryOverlay(reg, segments, outline, alpha);
    });
    slug->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        INFO(reg.get<SlugResource>(entity).message);
        REQUIRE(reg.any_of<OverlayVectorFallback>(entity));
        CHECK_FALSE(reg.get<SlugResource>(entity).ready);
        CHECK(reg.get<SlugResource>(entity).layers.empty());
        CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
    });
    const auto firstMessage = messages.text.str();
    CHECK(firstMessage.find("info: SlugSystemNode:") != std::string::npos);
    CHECK(firstMessage.find("rocky-overlay-1") != std::string::npos);
    CHECK(firstMessage.find("8-band-per-axis performance floor") != std::string::npos);
    CHECK(firstMessage.find("falling back to Raster") != std::string::npos);
    for (unsigned frame = 0u; frame < 3u; ++frame)
    {
        bake->update(context.get());
        slug->update(context.get());
    }
    registry.read([&](entt::registry& reg) { CHECK(reg.any_of<RenderTexture>(entity)); });
    CHECK(messages.text.str() == firstMessage);
}

//! Verifies the remaining Canvas commit sites share the capacity policy, so
//! point collections and legacy Mesh fills cannot bypass reduction or fallback.
TEST_CASE("slug canvas fills share capacity handling", "[slug][overlay-fallback]")
{
    bool meshFill = false, overflow = false;
    SECTION("point within capacity") {}
    SECTION("point fallback") { overflow = true; }
    SECTION("mesh reduction") { meshFill = true; }
    SECTION("mesh fallback") { meshFill = true; overflow = true; }
    INFO("meshFill=" << meshFill << ", overflow=" << overflow);

    LogCapture messages;
    Registry registry = Registry::create();
    auto slug = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
        // Fine-tolerance circles contain many more generated curves than the
        // three straight curves in a triangle; keep both fixtures bounded.
        const unsigned count = meshFill ? (overflow ? 23000u : 3072u) : (overflow ? 128u : 64u);
        if (meshFill)
        {
            auto& geometry = reg.emplace<MeshGeometry>(entity);
            for (unsigned i = 0u; i < count; ++i)
            {
                const double x = -0.35 + 0.7 * i / count;
                geometry.vertices.emplace_back(x, -0.4, 0.0);
                geometry.vertices.emplace_back(x + 0.00001, -0.4, 0.0);
                geometry.vertices.emplace_back(x, 0.4, 0.0);
            }
            reg.emplace<Mesh>(entity, geometry);
        }
        else
        {
            auto& geometry = reg.emplace<PointGeometry>(entity);
            for (unsigned i = 0u; i < count; ++i)
                geometry.points.emplace_back(0.1 * i / count, 0.0, 0.0);
            auto& style = reg.emplace<PointStyle>(entity);
            style.width = 256.0f;
            reg.emplace<Point>(entity, geometry, style);
        }
    });
    slug->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        INFO(resource.message);
        CHECK(reg.any_of<OverlayVectorFallback>(entity) == overflow);
        REQUIRE(resource.ready == !overflow);
        if (!overflow)
        {
            REQUIRE(resource.layers.size() == 1u);
            if (meshFill)
            {
                CHECK(resource.layers.front().shapeData.z < 31u);
                CHECK(resource.layers.front().shapeData.w < 31u);
                CHECK(resource.layers.front().shapeData.z >= 7u);
                CHECK(resource.layers.front().shapeData.w >= 7u);
            }
            else
            {
                CHECK(resource.layers.front().shapeData.z == 31u);
                CHECK(resource.layers.front().shapeData.w == 31u);
            }
        }
    });
    if (overflow || meshFill)
        CHECK(messages.text.str().find(overflow ? "falling back to Raster" : "reduced bands") != std::string::npos);
    else
        CHECK(messages.text.str().empty());
}

//! Keeps accepted reductions at 16/8 bands, sends shapes needing 4/1 bands to
//! Raster, and leaves small shapes' naturally lower band counts unchanged.
TEST_CASE("slug polygon capacity recovery stops at eight bands", "[projection][slug][overlay-fallback]")
{
    unsigned holes = 2048u;
    unsigned expectedBands = 8u; // Zero denotes Raster fallback.
    SECTION("eight bands fit") {}
    SECTION("sixteen bands fit") { holes = 1024u; expectedBands = 16u; }
    SECTION("four bands would be required") { holes = 4096u; expectedBands = 0u; }
    SECTION("one band would be required") { holes = 12000u; expectedBands = 0u; }
    SECTION("small shapes keep their natural band count") { holes = 0u; expectedBands = 2u; }

    LogCapture messages;
    Registry registry = Registry::create();
    auto slug = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.polygons.push_back(slottedPolygon(holes));
        reg.emplace<rocky::Polygon>(entity, geometry);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });
    slug->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        INFO(resource.message);
        CHECK(reg.any_of<OverlayVectorFallback>(entity) == (expectedBands == 0u));
        REQUIRE(resource.ready == (expectedBands != 0u));
        if (expectedBands != 0u)
        {
            REQUIRE(resource.layers.size() == 1u);
            CHECK(resource.layers.front().shapeData.z == expectedBands - 1u);
            CHECK(resource.layers.front().shapeData.w == expectedBands - 1u);
        }
        else
        {
            CHECK(resource.layers.empty());
            CHECK_FALSE(resource.curveTexture);
            CHECK_FALSE(resource.bandTexture);
            CHECK(resource.message.find("8-band-per-axis performance floor") != std::string::npos);
        }
        CHECK(reg.get<PolygonGeometry>(entity).polygons.front().holes.size() == holes);
    });
    const auto firstMessage = messages.text.str();
    if (expectedBands == 0u)
    {
        CHECK(firstMessage.find("info: SlugSystemNode:") != std::string::npos);
        CHECK(firstMessage.find("falling back to Raster") != std::string::npos);
        CHECK(firstMessage.find("reduced bands") == std::string::npos);
    }
    else if (holes != 0u)
    {
        CHECK(firstMessage.find("info: SlugSystemNode: overlay ") != std::string::npos);
        CHECK(firstMessage.find("reduced bands per axis from 32 to " + std::to_string(expectedBands)) != std::string::npos);
    }
    else
        CHECK(firstMessage.empty());
    for (unsigned frame = 0u; frame < 3u; ++frame)
        slug->update(context.get());
    CHECK(messages.text.str() == firstMessage);
}

//! Exercises capacity classification, per-overlay Raster routing, sticky
//! failure, automatic-refit immunity, recovery, and component-only teardown.
TEST_CASE("slug band exhaustion falls back once and recovers on source edits",
    "[projection][slug][overlay-fallback]")
{
    LogCapture warnings;
    Registry registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    bake->bakeScene = {}; // Test routing without allocating Vulkan render targets.
    auto slug = SlugSystemNode::create(registry);
    slug->worldSRS = SRS::SPHERICAL_MERCATOR;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null, unaffected = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.srs = SRS::SPHERICAL_MERCATOR;
        geometry.polygons.push_back(slottedPolygon(17000u));
        reg.emplace<rocky::Polygon>(entity, geometry);
        Overlay overlay;
        overlay.mode = OverlayMode::Vector;
        overlay.resolution = { 256u, 128u };
        reg.emplace<Overlay>(entity, overlay);
        reg.emplace<Transform>(entity).position = GeoPoint(SRS::SPHERICAL_MERCATOR, 0.0, 0.0, 0.0);
        reg.emplace<AutoOverlayTransform>(entity);

        unaffected = reg.create();
        auto& simple = reg.emplace<PolygonGeometry>(unaffected);
        simple.polygons.push_back(slottedPolygon(0u));
        reg.emplace<rocky::Polygon>(unaffected, simple);
        reg.emplace<Overlay>(unaffected, overlay);
    });
    slug->update(context.get());
    std::uint64_t generation = 0u;
    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<OverlayVectorFallback>(entity));
        const auto& resource = reg.get<SlugResource>(entity);
        CHECK_FALSE(resource.ready);
        CHECK_FALSE(resource.curveTexture);
        CHECK_FALSE(resource.bandTexture);
        CHECK(resource.layers.empty());
        CHECK(resource.message.find("8-band-per-axis performance floor") != std::string::npos);
        generation = resource.atlasGeneration;
        CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
        CHECK(resolveOverlayMode(reg, entity, OverlayMode::Vector) == OverlayMode::Raster);
        CHECK(resolveOverlayMode(reg, unaffected, OverlayMode::Vector) == OverlayMode::Vector);
        CHECK(reg.get<SlugResource>(unaffected).ready);
    });
    const auto firstWarning = warnings.text.str();
    CHECK(firstWarning.find("info: SlugSystemNode:") != std::string::npos);
    REQUIRE(firstWarning.find("falling back to Raster") != std::string::npos);
    for (unsigned frame = 0u; frame < 4u; ++frame)
    {
        registry.write([&](entt::registry& reg) { reg.get<Transform>(entity).dirty(); });
        bake->update(context.get());
        slug->update(context.get());
    }
    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<RenderTexture>(entity));
        CHECK(reg.get<RenderTexture>(entity).textureSize == glm::uvec2(256u, 128u));
        CHECK(reg.get<RenderParticipation>(entity).renderTexture);
        CHECK_FALSE(reg.get<RenderParticipation>(entity).mainView);
        CHECK(reg.get<SlugResource>(entity).atlasGeneration == generation);
        CHECK_FALSE(reg.any_of<RenderTexture>(unaffected));
    });
    CHECK(warnings.text.str() == firstWarning);

    SECTION("diagnostic export completes without retrying a failed atlas")
    {
        registry.write([&](entt::registry& reg)
        {
            reg.get<SlugResource>(entity).exportPath = "not-created.slug";
        });
        slug->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            const auto& resource = reg.get<SlugResource>(entity);
            CHECK(resource.exportPath.empty());
            CHECK_FALSE(resource.exportSucceeded);
            CHECK(resource.exportMessage.find("using Raster") != std::string::npos);
            CHECK(resource.atlasGeneration == generation);
        });
    }
    SECTION("geometry changes permit another vector build")
    {
        registry.write([&](entt::registry& reg)
        {
            auto& geometry = reg.get<PolygonGeometry>(entity);
            geometry.polygons.front().holes.clear();
            geometry.srs = SRS::EMPTY;
            PolygonGeometry::dirty(reg, entity);
        });
        slug->update(context.get()); // Release sticky decision; allow the refit first.
        bake->update(context.get());
        slug->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            CHECK_FALSE(reg.any_of<OverlayVectorFallback>(entity));
            CHECK_FALSE(reg.any_of<RenderTexture>(entity));
            CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
            CHECK(reg.get<SlugResource>(entity).ready);
            CHECK(reg.get<SlugResource>(entity).atlasGeneration > generation);
        });
    }
    SECTION("caller can select raster explicitly")
    {
        registry.write([&](entt::registry& reg) { reg.get<Overlay>(entity).mode = OverlayMode::Raster; });
        slug->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            CHECK_FALSE(reg.any_of<OverlayVectorFallback>(entity));
            CHECK_FALSE(reg.any_of<SlugResource>(entity));
            CHECK(reg.any_of<RenderTexture>(entity));
        });
    }
    SECTION("removing the overlay removes its private state")
    {
        registry.write([&](entt::registry& reg)
        {
            reg.remove<Overlay>(entity);
            CHECK_FALSE(reg.any_of<OverlayVectorFallback>(entity));
            CHECK_FALSE(reg.any_of<SlugResource>(entity));
            CHECK_FALSE(reg.any_of<RenderTexture>(entity));
            CHECK(reg.any_of<PolygonGeometry>(entity));
        });
    }
}

//! Raster fallback is reserved for the known capacity condition, not malformed
//! coordinates or other unsupported authoring inputs.
TEST_CASE("slug authoring errors do not activate capacity fallback", "[projection][slug][overlay-fallback]")
{
    Registry registry = Registry::create();
    auto slug = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.polygons.push_back(slottedPolygon(0u));
        geometry.polygons.front().outer.front().x = std::numeric_limits<double>::quiet_NaN();
        reg.emplace<rocky::Polygon>(entity, geometry);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });
    slug->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.get<SlugResource>(entity).ready);
        CHECK_FALSE(reg.any_of<OverlayVectorFallback>(entity));
        CHECK(resolveOverlayMode(reg, entity, OverlayMode::Vector) == OverlayMode::Vector);
    });
}

//! Tests the shared fallback decision's effect on derived meshes separately
//! from the huge capacity fixture, keeping headless tessellation inexpensive.
TEST_CASE("polygon raster fallback derives and retires its mesh without dirtying the source",
    "[projection][slug][overlay-fallback]")
{
    Registry registry = Registry::create();
    auto polygon = PolygonSystemNode::create(registry);
    // MeshSystem assigns ownership to the generated geometry/style components,
    // as it does in the application's normal system collection.
    auto mesh = MeshSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.polygons.push_back(slottedPolygon(1u));
        reg.emplace<rocky::Polygon>(entity, geometry);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });
    polygon->update(context.get());
    std::uint64_t revision = 0u;
    registry.write([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        revision = reg.get<rocky::Polygon>(entity).componentRevision();
        reg.emplace<OverlayVectorFallback>(entity);
    });
    polygon->update(context.get());
    entt::entity meshGeometry = entt::null, meshStyle = entt::null;
    registry.write([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<Mesh>(entity));
        const auto& mesh = reg.get<Mesh>(entity);
        meshGeometry = mesh.geometry;
        meshStyle = mesh.style;
        CHECK(reg.any_of<MeshGeometry>(meshGeometry));
        CHECK(reg.get<rocky::Polygon>(entity).componentRevision() == revision);
        reg.remove<OverlayVectorFallback>(entity);
    });
    polygon->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        CHECK_FALSE(reg.valid(meshGeometry));
        CHECK_FALSE(reg.valid(meshStyle));
        CHECK(reg.get<rocky::Polygon>(entity).componentRevision() == revision);
        CHECK(reg.get<PolygonGeometry>(entity).polygons.front().holes.size() == 1u);
    });
}
#endif
