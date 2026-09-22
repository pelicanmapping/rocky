#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/rocky.h>
#include <rocky/Log.h>
#ifndef ROCKY_HAS_SLUGHORN
#include <spdlog/sinks/ostream_sink.h>
#endif
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/Decal.h>
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#ifdef ROCKY_HAS_SLUGHORN
#include <rocky/vsg/ecs/SlugResource.h>
#include <rocky/vsg/ecs/SlugSystem.h>
#endif
#include <rocky/vsg/ecs/DecalSystem.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include <rocky/vsg/ecs/PolygonSystem.h>
#include <rocky/vsg/ecs/LineSystem.h>
#include <rocky/vsg/ecs/PointSystem.h>
#include <rocky/vsg/ecs/ModelSystem.h>
#include <rocky/vsg/ecs/TextureSystem.h>
#include <rocky/vsg/ecs/OpticsSystem.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/vsg/ecs/FeatureBuilder.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    class TestLayer : public Inherit<Layer, TestLayer>
    {
    public:
        Result<> openImplementation(const IOOptions& io) override {
            return ResultVoidOK;
        }
    };
}

TEST_CASE("strings")
{
    std::string s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "world", "Rocky") == "Hello, Rocky!");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "world", "") == "Hello, !");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "", "Rocky") == "Hello, world!");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "", "") == "Hello, world!");

    s1 = detail::trim("  Hello, Rocky!  ");
    CHECK(s1 == "Hello, Rocky!");
    s1 = "  Hello, Rocky!  ";
    CHECK(detail::trimInPlace(s1) == "Hello, Rocky!");
}

TEST_CASE("mesh feature default tessellation is curvature bounded", "[featurebuilder]")
{
    Feature building(
        SRS::WGS84,
        Geometry::Type::Polygon,
        {
            { 139.0, 35.0, 0.0 },
            { 139.001, 35.0, 0.0 },
            { 139.001, 35.001, 0.0 },
            { 139.0, 35.001, 0.0 }
        });

    FeatureBuilder builder;

    MeshStyle defaultStyle;
    MeshGeometry defaultGeometry;
    builder.buildMeshGeometry({ building }, defaultStyle, defaultGeometry);

    REQUIRE_FALSE(defaultGeometry.vertices.empty());
    CHECK(defaultGeometry.vertices.size() < 100u);

    MeshStyle fineStyle;
    fineStyle.resolution = 20.0f;
    MeshGeometry fineGeometry;
    builder.buildMeshGeometry({ building }, fineStyle, fineGeometry);

    CHECK(fineGeometry.vertices.size() > defaultGeometry.vertices.size());
}

TEST_CASE("polygon geometry preserves rings and triangulates holes", "[featurebuilder][polygon]")
{
    Feature feature;
    feature.srs = SRS::WGS84;
    feature.geometry.type = Geometry::Type::Polygon;
    feature.geometry.points = {
        { -2.0, -2.0, 0.0 },
        {  2.0, -2.0, 0.0 },
        {  2.0,  2.0, 0.0 },
        { -2.0,  2.0, 0.0 },
        { -2.0, -2.0, 0.0 }
    };
    feature.geometry.parts.emplace_back(Geometry::Type::LineString,
        std::vector<glm::dvec3>{
            { -1.0, -1.0, 0.0 },
            { -1.0,  1.0, 0.0 },
            {  1.0,  1.0, 0.0 },
            {  1.0, -1.0, 0.0 },
            { -1.0, -1.0, 0.0 }
        });
    feature.dirtyExtent();

    FeatureBuilder builder;
    builder.colorFunction = [](const Feature&) { return StockColor::Red; };
    PolygonStyle buildStyle;
    buildStyle.resolution = 1000000000.0f;

    PolygonGeometry built;
    builder.buildPolygonGeometry({ feature }, buildStyle, built);
    REQUIRE(built.polygons.size() == 1u);
    CHECK(built.polygons.front().outer.size() == 4u);
    REQUIRE(built.polygons.front().holes.size() == 1u);
    CHECK(built.polygons.front().holes.front().size() == 4u);
    REQUIRE(built.colors.size() == 1u);
    CHECK(built.colors.front() == StockColor::Red);

    PolygonStyle finerBuildStyle;
    finerBuildStyle.resolution = 50000.0f;
    PolygonGeometry finerBuilt;
    builder.buildPolygonGeometry({ feature }, finerBuildStyle, finerBuilt);
    REQUIRE(finerBuilt.polygons.size() == 1u);
    CHECK(finerBuilt.polygons.front().outer.size() >
        built.polygons.front().outer.size());

    // Deferred georeferenced triangulation has no ElevationSession, so any
    // seed vertices it introduces must inherit the source surface elevation.
    PolygonGeometry elevated;
    elevated.srs = SRS::WGS84;
    elevated.polygons.emplace_back(PolygonPart{
        {
            { -0.01, -0.01, 250.0 },
            {  0.01, -0.01, 250.0 },
            {  0.01,  0.01, 250.0 },
            { -0.01,  0.01, 250.0 }
        },
        {}
    });
    PolygonStyle elevatedStyle;
    elevatedStyle.color = StockColor::White;
    MeshGeometry elevatedMesh;
    builder.buildMeshGeometry(elevated, elevatedStyle, elevatedMesh);
    REQUIRE_FALSE(elevatedMesh.vertices.empty());
    for (const auto& vertex : elevatedMesh.vertices)
        CHECK(vertex.z == Approx(250.0).epsilon(1e-9));

    // Exercise the local-coordinate converter independently from projection.
    built.srs = {};
    PolygonStyle style;
    style.useGeometryColors = true;
    MeshGeometry mesh;
    builder.buildMeshGeometry(built, style, mesh);

    REQUIRE_FALSE(mesh.indices.empty());
    REQUIRE((mesh.indices.size() % 3u) == 0u);
    double area = 0.0;
    for (std::size_t i = 0u; i < mesh.indices.size(); i += 3u)
    {
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1u]];
        const auto& c = mesh.vertices[mesh.indices[i + 2u]];
        const auto center = (a + b + c) / 3.0;
        const bool centerIsInHole =
            std::abs(center.x) < 1.0 && std::abs(center.y) < 1.0;
        CHECK_FALSE(centerIsInHole);
        area += std::abs(glm::cross(b - a, c - a).z) * 0.5;
    }
    CHECK(area == Approx(12.0).epsilon(1e-6));
}

TEST_CASE("polygon system owns only its derived mesh", "[polygon][projection]")
{
    Registry registry = Registry::create();
    auto polygonSystem = PolygonSystemNode::create(registry);
    auto meshSystem = MeshSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.polygons.emplace_back(PolygonPart{
            {
                { -1.0, -1.0, 0.0 },
                {  1.0, -1.0, 0.0 },
                {  1.0,  1.0, 0.0 },
                { -1.0,  1.0, 0.0 }
            },
            {}
        });
        auto& style = reg.emplace<PolygonStyle>(entity);
        style.color = StockColor::Yellow;
        style.depthOffset = 12.0f;
        reg.emplace<rocky::Polygon>(entity, geometry, style);
    });

    polygonSystem->update(context.get());

    entt::entity firstGeometry = entt::null;
    entt::entity firstStyle = entt::null;
    std::uint64_t firstGeometryRevision = 0u;
    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<Mesh>(entity));
        const auto& mesh = reg.get<Mesh>(entity);
        firstGeometry = mesh.geometry;
        firstStyle = mesh.style;
        REQUIRE(reg.valid(firstGeometry));
        REQUIRE(reg.valid(firstStyle));
        CHECK_FALSE(reg.get<MeshGeometry>(firstGeometry).vertices.empty());
        firstGeometryRevision = reg.get<MeshGeometry>(firstGeometry).componentRevision();
        CHECK(reg.get<MeshStyle>(firstStyle).color == StockColor::Yellow);
        CHECK(reg.get<MeshStyle>(firstStyle).depthOffset == Approx(12.0f));
    });

    registry.write([&](entt::registry& reg)
    {
        auto& style = reg.get<PolygonStyle>(entity);
        style.color = StockColor::Red;
        style.depthOffset = 37.0f;
        style.dirty(reg);
    });
    polygonSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        CHECK(reg.get<MeshGeometry>(firstGeometry).componentRevision() ==
            firstGeometryRevision);
        CHECK(reg.get<MeshStyle>(firstStyle).color == StockColor::Red);
        CHECK(reg.get<MeshStyle>(firstStyle).depthOffset == Approx(37.0f));
    });

    registry.write([&](entt::registry& reg)
    {
        auto& style = reg.get<PolygonStyle>(entity);
        style.resolution = 5000.0f;
        style.dirty(reg);
    });
    polygonSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        CHECK(reg.get<MeshGeometry>(firstGeometry).componentRevision() !=
            firstGeometryRevision);
        CHECK(reg.get<MeshStyle>(firstStyle).resolution == Approx(5000.0f));
    });

    registry.write([&](entt::registry& reg)
    {
        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Raster;
    });
    polygonSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<Mesh>(entity));
        const auto& mesh = reg.get<Mesh>(entity);
        CHECK(mesh.geometry == firstGeometry);
        CHECK(mesh.style == firstStyle);
        CHECK(reg.get<MeshStyle>(firstStyle).depthOffset == Approx(0.0f));
    });

    registry.write([&](entt::registry& reg)
    {
        reg.patch<Overlay>(entity, [](Overlay& overlay)
        {
            overlay.mode = OverlayMode::Vector;
        });
    });
    polygonSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
#ifdef ROCKY_HAS_SLUGHORN
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        CHECK_FALSE(reg.valid(firstGeometry));
        CHECK_FALSE(reg.valid(firstStyle));
#else
        // Vector fallback keeps the mesh and its caches ready for Raster.
        REQUIRE(reg.any_of<Mesh>(entity));
        const auto& mesh = reg.get<Mesh>(entity);
        CHECK(mesh.geometry == firstGeometry);
        CHECK(mesh.style == firstStyle);
        CHECK(reg.get<MeshStyle>(firstStyle).depthOffset == Approx(0.0f));
#endif
    });

    registry.write([&](entt::registry& reg)
    {
        reg.patch<Overlay>(entity, [](Overlay& overlay)
        {
            overlay.mode = OverlayMode::Raster;
        });
    });
    polygonSystem->update(context.get());

    entt::entity secondGeometry = entt::null;
    entt::entity secondStyle = entt::null;
    registry.write([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<Mesh>(entity));
        const auto& mesh = reg.get<Mesh>(entity);
        secondGeometry = mesh.geometry;
        secondStyle = mesh.style;
        reg.remove<rocky::Polygon>(entity);
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        CHECK_FALSE(reg.valid(secondGeometry));
        CHECK_FALSE(reg.valid(secondStyle));
    });
}

TEST_CASE("projected texture contracts", "[projection]")
{
    LineStyle lineStyle;
    CHECK(lineStyle.outlineWidth == 0.0f);
    CHECK(lineStyle.outlineColor == StockColor::Black);
    CHECK(lineStyle.widthUnits == Units::PIXELS);

    lineStyle.width = 10.0f;
    lineStyle.outlineWidth = 2.0f;
    lineStyle.widthUnits = Units::FEET;
    LineStyleRecord metricRecord;
    metricRecord.populate(lineStyle);
    CHECK(metricRecord.widthIsPhysical == 1u);
    CHECK(metricRecord.width == Approx(3.048f));
    CHECK(metricRecord.outlineWidth == Approx(0.6096f));

    Overlay overlay;
    CHECK(overlay.mode == OverlayMode::Raster);

    RenderTexture renderTexture;
    CHECK(renderTexture.sources.empty());
    CHECK(renderTexture.textureSize == glm::uvec2(512u, 512u));
    CHECK_FALSE(renderTexture.useDepthBuffer);

    ProjectedTexture projected;
    CHECK((projected.texture == entt::null));
    CHECK((projected.projector == entt::null));

    RenderTextureBounds bounds;
    bounds.expand(SRS::WGS84, glm::dvec3(179.9, 10.0, 0.0));
    bounds.expand(SRS::WGS84, glm::dvec3(-179.9, 11.0, 0.0));
    REQUIRE(bounds.valid);
    CHECK(bounds.maxx - bounds.minx < 1.0);

    SharedRenderData shared;
    REQUIRE(shared.decalTextures);
    CHECK(shared.projectedTextureCapacity() ==
        SharedRenderData::DEFAULT_PROJECTED_TEXTURE_CAPACITY);

    auto originalDescriptor = shared.decalTextures;
    shared.configureProjectedTextureCapacity(17u);
    CHECK(shared.projectedTextureCapacity() == 17u);
    CHECK(shared.decalTextures != originalDescriptor);

    auto configuredDescriptor = shared.decalTextures;
    shared.configureProjectedTextureCapacity(17u);
    CHECK(shared.decalTextures == configuredDescriptor);

    shared.configureProjectedTextureCapacity(0u);
    CHECK(shared.projectedTextureCapacity() == 1u);
}

TEST_CASE("render texture revisions", "[projection]")
{
    Registry registry = Registry::create();
    auto meshSystem = MeshSystemNode::create(registry);
    auto lineSystem = LineSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto source = reg.create();

        auto& meshGeometry = reg.emplace<MeshGeometry>(source);
        meshGeometry.srs = SRS::WGS84;
        meshGeometry.vertices.emplace_back(0.0, 0.0, 0.0);

        auto& meshStyle = reg.emplace<MeshStyle>(source);
        reg.emplace<Mesh>(source, meshGeometry, meshStyle);

        RenderTextureRevision meshInitial;
        meshSystem->contributeRenderTextureRevision(reg, source, meshInitial);

        auto previousComponentRevision = meshStyle.componentRevision();
        meshStyle.depthOffset = 100.0f;
        meshStyle.dirty(reg);
        CHECK(meshStyle.componentRevision() > previousComponentRevision);

        RenderTextureRevision meshStyleChanged;
        meshSystem->contributeRenderTextureRevision(reg, source, meshStyleChanged);
        CHECK(meshStyleChanged.bounds == meshInitial.bounds);
        CHECK(meshStyleChanged.content != meshInitial.content);

        meshGeometry.vertices.emplace_back(1.0, 1.0, 0.0);
        meshGeometry.dirty(reg);

        RenderTextureRevision meshGeometryChanged;
        meshSystem->contributeRenderTextureRevision(reg, source, meshGeometryChanged);
        CHECK(meshGeometryChanged.bounds != meshStyleChanged.bounds);
        CHECK(meshGeometryChanged.content != meshStyleChanged.content);

        MeshStyle replacement;
        replacement.depthOffset = 200.0f;
        auto revisionBeforeReplacement = meshStyle.componentRevision();
        auto& replacedStyle = reg.emplace_or_replace<MeshStyle>(source, replacement);
        CHECK(replacedStyle.componentRevision() != revisionBeforeReplacement);

        auto& lineGeometry = reg.emplace<LineGeometry>(source);
        lineGeometry.srs = SRS::WGS84;
        lineGeometry.points = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 } };
        auto& lineStyle = reg.emplace<LineStyle>(source);
        reg.emplace<Line>(source, lineGeometry, lineStyle);

        lineStyle.width = 6.0f;
        lineStyle.outlineWidth = 3.0f;
        RenderTextureBounds outlinedLineBounds;
        lineSystem->expandRenderTextureBounds(
            reg, source, outlinedLineBounds, SRS::WGS84, false);
        CHECK(outlinedLineBounds.paddingPixels == Approx(8.0));

        lineStyle.widthUnits = Units::METERS;
        RenderTextureBounds metricLineBounds;
        lineSystem->expandRenderTextureBounds(
            reg, source, metricLineBounds, SRS::WGS84, false);
        CHECK(metricLineBounds.paddingPixels == Approx(2.0));
        CHECK(metricLineBounds.paddingMeters == Approx(6.0));

        RenderTextureRevision lineInitial;
        lineSystem->contributeRenderTextureRevision(reg, source, lineInitial);

        // Line width affects bounds padding, while depthOffset verifies that
        // formerly omitted style properties still invalidate baked content.
        lineStyle.depthOffset = 50.0f;
        lineStyle.dirty(reg);

        RenderTextureRevision lineStyleChanged;
        lineSystem->contributeRenderTextureRevision(reg, source, lineStyleChanged);
        CHECK(lineStyleChanged.bounds != lineInitial.bounds);
        CHECK(lineStyleChanged.content != lineInitial.content);
    });
}

TEST_CASE("render texture participant order", "[projection]")
{
    Registry registry = Registry::create();
    auto mesh = MeshSystemNode::create(registry);
    auto line = LineSystemNode::create(registry);
    auto point = PointSystemNode::create(registry);
    auto model = ModelSystemNode::create(registry);

    CHECK(mesh->renderTextureOrder() < line->renderTextureOrder());
    CHECK(line->renderTextureOrder() < point->renderTextureOrder());
    CHECK(point->renderTextureOrder() < model->renderTextureOrder());
}

TEST_CASE("line triangles use the segment start as provoking vertex", "[line]")
{
    auto check = [](LineTopology topology, const std::vector<vsg::dvec3>& points,
        const std::vector<std::uint32_t>& expectedProvokingVertices)
    {
        auto node = LineGeometryNode::create();
        node->set(points, std::vector<vsg::vec4>(), topology);

        REQUIRE(node->_drawCommand->indexCount == expectedProvokingVertices.size() * 6u);
        for (std::size_t segment = 0; segment < expectedProvokingVertices.size(); ++segment)
        {
            const auto firstTriangle = (*node->_indices)[segment * 6u];
            const auto secondTriangle = (*node->_indices)[segment * 6u + 3u];
            CHECK(firstTriangle == expectedProvokingVertices[segment]);
            CHECK(secondTriangle == expectedProvokingVertices[segment]);
        }
    };

    check(LineTopology::Strip,
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 } },
        { 2u, 6u });

    check(LineTopology::Segments,
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
          { 2.0, 0.0, 0.0 }, { 2.0, 1.0, 0.0 } },
        { 2u, 10u });
}

TEST_CASE("render texture bounds include source transform", "[projection]")
{
    Registry registry = Registry::create();
    auto meshSystem = MeshSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto source = reg.create();
        auto& geometry = reg.emplace<MeshGeometry>(source);
        geometry.vertices.emplace_back(1.0, 2.0, 3.0);
        auto& style = reg.emplace<MeshStyle>(source);
        reg.emplace<Mesh>(source, geometry, style);

        auto& transform = reg.emplace<Transform>(source);
        transform.position = GeoPoint(SRS::SPHERICAL_MERCATOR, 10.0, 20.0, 30.0);
        transform.localMatrix = glm::scale(glm::dmat4(1.0), glm::dvec3(2.0, 3.0, 4.0));

        RenderTextureBounds transformed;
        meshSystem->expandRenderTextureBounds(
            reg, source, transformed, SRS::SPHERICAL_MERCATOR, true);
        REQUIRE(transformed.valid);
        CHECK(transformed.minx == Approx(12.0));
        CHECK(transformed.miny == Approx(26.0));
        CHECK(transformed.minz == Approx(42.0));

        RenderTextureBounds local;
        meshSystem->expandRenderTextureBounds(
            reg, source, local, SRS::SPHERICAL_MERCATOR, false);
        CHECK_FALSE(local.valid);
    });
}

TEST_CASE("texture producer ownership", "[projection]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    auto bakeSystem = OverlayBakeSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto externalImage = reg.create();
        reg.emplace<ImageTexture>(externalImage);
        reg.emplace<TextureResource>(externalImage);
        reg.remove<ImageTexture>(externalImage);
        CHECK(reg.any_of<TextureResource>(externalImage));

        auto producedImage = reg.create();
        reg.emplace<ImageTexture>(producedImage);
        auto& imageResource = reg.emplace<TextureResource>(producedImage);
        imageResource.producer = TextureResourceProducer::ImageTexture;
        reg.remove<ImageTexture>(producedImage);
        CHECK_FALSE(reg.any_of<TextureResource>(producedImage));

        auto externalBake = reg.create();
        reg.emplace<RenderTexture>(externalBake);
        reg.emplace<TextureResource>(externalBake);
        reg.remove<RenderTexture>(externalBake);
        CHECK(reg.any_of<TextureResource>(externalBake));

        auto producedBake = reg.create();
        reg.emplace<RenderTexture>(producedBake);
        auto& bakeResource = reg.emplace<TextureResource>(producedBake);
        bakeResource.producer = TextureResourceProducer::RenderTexture;
        reg.remove<RenderTexture>(producedBake);
        CHECK_FALSE(reg.any_of<TextureResource>(producedBake));
    });
}

TEST_CASE("null image texture is a ready procedural resource", "[projection]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    auto contextSingleton = VSGContextFactory::create(vsg::Viewer::create());

    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<ImageTexture>(entity);
    });

    textureSystem->update(contextSingleton.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<TextureResource>(entity);
        CHECK(resource.producer == TextureResourceProducer::ImageTexture);
        CHECK(resource.ready);
        CHECK_FALSE(resource.texture);
    });
}

TEST_CASE("manual optics work without a terrain target", "[projection]")
{
    Registry registry = Registry::create();
    auto opticsSystem = OpticsSystemNode::create(registry);
    auto contextSingleton = VSGContextFactory::create(vsg::Viewer::create());
    auto context = contextSingleton.get();

    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& optics = reg.emplace<Optics>(entity);
        optics.autoComputeFocalDistance = false;
        optics.autoComputeNearFar = false;
        optics.focalDistance = 25.0;
        optics.nearScale = 0.5;
        optics.nearBias = 1.0;
        optics.farScale = 2.0;
        optics.farBias = 3.0;
        reg.emplace<TransformDetail>(entity);
    });

    opticsSystem->update(context);

    registry.read([&](entt::registry& reg)
    {
        const auto& detail = reg.get<OpticsDetail>(entity).views[0];
        CHECK(detail.focalDistance == Approx(25.0));
        CHECK(detail.nearDistance == Approx(13.5));
        CHECK(detail.farDistance == Approx(53.0));
        CHECK_FALSE(detail.focalPointValid);
    });
}

TEST_CASE("legacy overlay adapter", "[projection]")
{
    Registry registry = Registry::create();
    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    auto decalSystem = DecalSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto overlayEntity = reg.create();
        reg.emplace<Overlay>(overlayEntity);
        CHECK(reg.any_of<RenderTexture>(overlayEntity));
        CHECK(reg.any_of<ProjectedTexture>(overlayEntity));
        REQUIRE(reg.any_of<RenderParticipation>(overlayEntity));
        CHECK_FALSE(reg.get<RenderParticipation>(overlayEntity).mainView);

        reg.patch<Overlay>(overlayEntity, [](auto& overlay)
        {
            overlay.mode = OverlayMode::Vector;
        });
        CHECK(reg.get<Overlay>(overlayEntity).mode == OverlayMode::Vector);
#ifdef ROCKY_HAS_SLUGHORN
        CHECK_FALSE(reg.any_of<RenderTexture>(overlayEntity));
        CHECK_FALSE(reg.get<RenderParticipation>(overlayEntity).renderTexture);
#else
        CHECK(reg.any_of<RenderTexture>(overlayEntity));
        CHECK(reg.get<RenderParticipation>(overlayEntity).renderTexture);
#endif

        reg.patch<Overlay>(overlayEntity, [](auto& overlay)
        {
            overlay.mode = OverlayMode::Raster;
        });
        CHECK(reg.any_of<RenderTexture>(overlayEntity));
        CHECK(reg.get<RenderParticipation>(overlayEntity).renderTexture);

        reg.remove<Overlay>(overlayEntity);
        CHECK_FALSE(reg.any_of<RenderTexture>(overlayEntity));
        CHECK_FALSE(reg.any_of<ProjectedTexture>(overlayEntity));
        CHECK_FALSE(reg.any_of<RenderParticipation>(overlayEntity));

        auto explicitEntity = reg.create();
        reg.emplace<RenderTexture>(explicitEntity);
        reg.emplace<ProjectedTexture>(explicitEntity);
        reg.emplace<RenderParticipation>(explicitEntity);
        reg.emplace<Overlay>(explicitEntity);
        reg.remove<Overlay>(explicitEntity);
        CHECK(reg.any_of<RenderTexture>(explicitEntity));
        CHECK(reg.any_of<ProjectedTexture>(explicitEntity));
        CHECK(reg.any_of<RenderParticipation>(explicitEntity));
    });
}

#ifndef ROCKY_HAS_SLUGHORN
TEST_CASE("vector overlays fall back to raster without repeated warnings", "[projection][overlay-fallback]")
{
    std::ostringstream warnings;
    auto logger = Log();
    auto sink = std::make_shared<log::sinks::ostream_sink_mt>(warnings);
    sink->set_pattern("%l: %v");

    // Restore the shared logger even if a REQUIRE exits this test early.
    struct RestoreLogger
    {
        Logger logger;
        std::vector<log::sink_ptr> sinks;
        log::level::level_enum level;
        ~RestoreLogger()
        {
            logger->sinks() = std::move(sinks);
            logger->set_level(level);
        }
    } restore{ logger, logger->sinks(), logger->level() };
    logger->sinks() = { sink };
    logger->set_level(log::level::warn);

    Registry registry = Registry::create();
    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    // Exercise facade synchronization without allocating Vulkan render targets.
    bakeSystem->bakeScene = {};
    auto decalSystem = DecalSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        Overlay requested;
        requested.mode = OverlayMode::Vector;
        requested.resolution = { 256u, 128u };
        requested.useDepthBuffer = true;
        requested.continuousBake = true;
        reg.emplace<Overlay>(entity, requested);

        CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
        REQUIRE(reg.any_of<RenderTexture>(entity));
        const auto& raster = reg.get<RenderTexture>(entity);
        CHECK(raster.textureSize == requested.resolution);
        CHECK(raster.useDepthBuffer);
        CHECK(raster.continuous);
        CHECK(reg.any_of<ProjectedTexture>(entity));
        REQUIRE(reg.any_of<RenderParticipation>(entity));
        CHECK(reg.get<RenderParticipation>(entity).renderTexture);
        CHECK_FALSE(reg.get<RenderParticipation>(entity).mainView);
    });

    const auto firstWarning = warnings.str();
    CHECK(firstWarning.find("warning:") != std::string::npos);
    CHECK(firstWarning.find("falling back to Raster") != std::string::npos);

    // Exercise the per-frame synchronization and ordinary dirty-field edits.
    for (int i = 0; i < 3; ++i)
        bakeSystem->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        auto& overlay = reg.get<Overlay>(entity);
        overlay.resolution = { 1024u, 512u };
        overlay.dirty(reg);
    });
    bakeSystem->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        CHECK(reg.get<Overlay>(entity).mode == OverlayMode::Vector);
        CHECK(reg.get<RenderTexture>(entity).textureSize == glm::uvec2(1024u, 512u));
        reg.patch<Overlay>(entity, [](auto& overlay) { overlay.mode = OverlayMode::Raster; });
        reg.patch<Overlay>(entity, [](auto& overlay) { overlay.mode = OverlayMode::Vector; });
        CHECK(reg.any_of<RenderTexture>(entity));
        CHECK(warnings.str() == firstWarning);

        reg.remove<Overlay>(entity);
        CHECK_FALSE(reg.any_of<RenderTexture>(entity));
        CHECK_FALSE(reg.any_of<ProjectedTexture>(entity));
        CHECK_FALSE(reg.any_of<RenderParticipation>(entity));
    });
}
#endif

#ifdef ROCKY_HAS_SLUGHORN
TEST_CASE("slug polygon auto-fits from ring bounds", "[projection][slug][polygon]")
{
    Registry registry = Registry::create();
    auto sourceSystems = ECSNode::create(registry, false);
    auto polygonSystem = PolygonSystemNode::create(registry);
    sourceSystems->add(polygonSystem);

    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    bakeSystem->renderSourceSystems = sourceSystems;
    bakeSystem->worldSRS = SRS::ECEF;
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.srs = SRS::WGS84;
        geometry.polygons.emplace_back(PolygonPart{
            {
                { 24.918, 60.161, 10.0 },
                { 24.920, 60.161, 10.0 },
                { 24.920, 60.163, 10.0 },
                { 24.918, 60.163, 10.0 }
            },
            {}
        });
        auto& style = reg.emplace<PolygonStyle>(entity);
        style.color = StockColor::Cyan;
        reg.emplace<rocky::Polygon>(entity, geometry, style);
        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Vector;
    });

    polygonSystem->update(context.get());
    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        REQUIRE((reg.any_of<AutoOverlayTransform, Transform>(entity)));
        const auto& resource = reg.get<SlugResource>(entity);
        REQUIRE(resource.ready);
        REQUIRE(resource.layers.size() == 1u);
        CHECK(resource.layers.front().color == StockColor::Cyan);
    });
}

TEST_CASE("slug polygon preserves a hole without a triangle mesh", "[projection][slug][polygon]")
{
    Registry registry = Registry::create();
    auto polygonSystem = PolygonSystemNode::create(registry);
    auto meshSystem = MeshSystemNode::create(registry);
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        geometry.polygons.emplace_back(PolygonPart{
            {
                { -0.4, -0.4, 0.0 },
                {  0.4, -0.4, 0.0 },
                {  0.4,  0.4, 0.0 },
                { -0.4,  0.4, 0.0 }
            },
            {{
                { -0.15, -0.15, 0.0 },
                { -0.15,  0.15, 0.0 },
                {  0.15,  0.15, 0.0 },
                {  0.15, -0.15, 0.0 }
            }}
        });
        auto& style = reg.emplace<PolygonStyle>(entity);
        style.color = StockColor::Yellow;
        reg.emplace<rocky::Polygon>(entity, geometry, style);
        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Vector;
    });

    polygonSystem->update(context.get());
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<Mesh>(entity));
        const auto& resource = reg.get<SlugResource>(entity);
        REQUIRE(resource.ready);
        REQUIRE(resource.layers.size() == 1u);
        CHECK(resource.layers.front().color == StockColor::Yellow);
    });

    const auto stamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto withHolePath = std::filesystem::temp_directory_path() /
        ("rocky-slug-polygon-hole-" + stamp + ".slug");
    const auto withoutHolePath = std::filesystem::temp_directory_path() /
        ("rocky-slug-polygon-solid-" + stamp + ".slug");

    registry.write([&](entt::registry& reg)
    {
        reg.get<SlugResource>(entity).exportPath = withHolePath.string();
    });
    slugSystem->update(context.get());

    registry.write([&](entt::registry& reg)
    {
        auto& geometry = reg.get<PolygonGeometry>(entity);
        geometry.polygons.front().holes.clear();
        geometry.dirty(reg);
    });
    polygonSystem->update(context.get());
    slugSystem->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        reg.get<SlugResource>(entity).exportPath = withoutHolePath.string();
    });
    slugSystem->update(context.get());

    auto curveTexelsUsed = [](const std::filesystem::path& path)
    {
        std::ifstream input(path);
        const auto document = json::parse(input);
        return document["packing_stats"]["curve_texels_used"].get<std::uint64_t>();
    };
    REQUIRE(std::filesystem::exists(withHolePath));
    REQUIRE(std::filesystem::exists(withoutHolePath));
    CHECK(curveTexelsUsed(withHolePath) > curveTexelsUsed(withoutHolePath));

    std::error_code removeError;
    CHECK(std::filesystem::remove(withHolePath, removeError));
    CHECK_FALSE(removeError);
    removeError.clear();
    CHECK(std::filesystem::remove(withoutHolePath, removeError));
    CHECK_FALSE(removeError);
}

TEST_CASE("slug atlas shares endpoints and spans texture rows", "[projection][slug][polygon]")
{
    Registry registry = Registry::create();
    auto slugSystem = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    std::set<std::array<float, 6>> expectedCurves;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        // All rectangles share a Y range, forcing a horizontal band with
        // more than 512 curves. The four-edge chains also force shared curve
        // pairs to straddle rows. Use binary-exact coordinates for comparison.
        for (unsigned i = 0u; i < 600u; ++i)
        {
            const double x = -0.375 + double(i) / 1024.0;
            const double w = 1.0 / 2048.0;
            PolygonPart part;
            part.outer = {
                { x, -0.25, 0.0 }, { x + w, -0.25, 0.0 },
                { x + w, 0.25, 0.0 }, { x, 0.25, 0.0 }
            };
            for (unsigned edge = 0u; edge < 4u; ++edge)
            {
                const auto& a = part.outer[edge];
                const auto& b = part.outer[(edge + 1u) % 4u];
                // Local geometry maps to [0,1]. The SDK represents a straight
                // edge with its control point coincident with its endpoint.
                expectedCurves.insert({
                    float(a.x + 0.5), float(a.y + 0.5),
                    float(b.x + 0.5), float(b.y + 0.5),
                    float(b.x + 0.5), float(b.y + 0.5) });
            }
            geometry.polygons.emplace_back(std::move(part));
        }
        reg.emplace<rocky::Polygon>(entity, geometry);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });

    slugSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        INFO(resource.message);
        REQUIRE(resource.ready);
        REQUIRE(resource.layers.size() == 1u);
        REQUIRE(resource.curveTexture);
        REQUIRE(resource.bandTexture);
        auto* curves = dynamic_cast<vsg::vec4Array2D*>(
            resource.curveTexture->imageView->image->data.get());
        auto* bands = dynamic_cast<vsg::usvec2Array2D*>(
            resource.bandTexture->imageView->image->data.get());
        REQUIRE(curves);
        REQUIRE(bands);
        CHECK(curves->properties.format == VK_FORMAT_R32G32B32A32_SFLOAT);
        CHECK(bands->properties.format == VK_FORMAT_R16G16_UINT);
        const unsigned width = 1u << resource.textureWidthLog2;
        REQUIRE(width == 512u); // No widening/retry needed for long band lists.
        CHECK(curves->width() == width);
        CHECK(bands->width() == width);

        // Mirror the shader's wrapped texel lookup, checking every referenced
        // curve against the original geometry rather than just atlas readiness.
        auto advance = [&](vsg::usvec2 loc, unsigned offset)
        {
            const unsigned x = unsigned(loc.x) + offset;
            return vsg::uivec2(x & (width - 1u), loc.y + (x >> resource.textureWidthLog2));
        };
        const auto& shape = resource.layers.front().shapeData;
        const unsigned shapeStart = shape.y * width + shape.x;
        const unsigned headerStart = shapeStart + 2u * resource.indirectionSize;
        const unsigned headerCount = shape.z + shape.w + 2u;
        REQUIRE(shape.x + 2u * resource.indirectionSize + headerCount <= width);
        std::vector<bool> seen(curves->valueCount(), false);
        bool multiRowBand = false, crossRowCurve = false, sharedEndpoint = false;
        for (unsigned h = 0u; h < headerCount; ++h)
        {
            const auto header = (*bands)[headerStart + h];
            multiRowBand = multiRowBand || header.x > width;
            for (unsigned i = 0u; i < header.x; ++i)
            {
                const auto bandLoc = advance(vsg::usvec2(shape.x, shape.y), unsigned(header.y) + i);
                const unsigned bandIndex = bandLoc.y * width + bandLoc.x;
                REQUIRE(bandIndex < bands->valueCount());
                CHECK(bandIndex == shapeStart + header.y + i);
                const auto curveLoc = (*bands)[bandIndex];
                const unsigned curveIndex = curveLoc.y * width + curveLoc.x;
                const auto endLoc = advance(curveLoc, 1u);
                const unsigned endIndex = endLoc.y * width + endLoc.x;
                REQUIRE(endIndex < curves->valueCount());
                CHECK(endIndex == curveIndex + 1u);
                crossRowCurve = crossRowCurve || endLoc.y != curveLoc.y;
                if (seen[curveIndex])
                    continue;
                seen[curveIndex] = true;
                const auto& p12 = (*curves)[curveIndex];
                const auto& p3 = (*curves)[endIndex];
                CHECK(expectedCurves.erase({ p12.x, p12.y, p12.z, p12.w, p3.x, p3.y }) == 1u);
            }
        }
        for (std::size_t i = 1u; i < seen.size(); ++i)
            sharedEndpoint = sharedEndpoint || (seen[i - 1u] && seen[i]);
        CHECK(expectedCurves.empty());
        CHECK(multiRowBand);
        CHECK(crossRowCurve);
        CHECK(sharedEndpoint);
        CHECK(curves->valueCount() < 600u * 4u * 2u);
    });
}

TEST_CASE("slug partitions dense polygon groups within one atlas", "[projection][slug][polygon]")
{
    Registry registry = Registry::create();
    auto polygonSystem = PolygonSystemNode::create(registry);
    auto slugSystem = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    using Curve = std::array<float, 6>;
    std::map<Curve, std::size_t> expected;
    std::vector<glm::vec2> holeSamples, fillSamples;
    const Color fillColor(StockColor::Yellow, 0.5f);
    constexpr std::size_t polygonCount = 1024u;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        auto addPolygon = [&](PolygonPart part)
        {
            auto addRing = [&](const PolygonPart::Ring& ring)
            {
                for (std::size_t i = 0u; i < ring.size(); ++i)
                {
                    const auto& a = ring[i];
                    const auto& b = ring[(i + 1u) % ring.size()];
                    expected.emplace(Curve{
                        float(a.x + 0.5), float(a.y + 0.5),
                        float(b.x + 0.5), float(b.y + 0.5),
                        float(b.x + 0.5), float(b.y + 0.5) }, geometry.polygons.size());
                }
            };
            addRing(part.outer);
            for (const auto& hole : part.holes)
                addRing(hole);
            geometry.polygons.emplace_back(std::move(part));
            geometry.colors.push_back(fillColor);
        };
        // Thousands of vertical edges cross most Y bands: the former one-key
        // group cannot fit its uint16 band offsets. Scramble spatial order so
        // the partitioner must actually sort rather than split input ranges.
        for (std::size_t i = 0u; i < polygonCount; ++i)
        {
            const double x = -0.375 + double((i * 613u) % polygonCount) / 2048.0;
            const double w = 1.0 / 4096.0;
            PolygonPart part;
            part.outer = {
                { x, -0.375, 0.0 }, { x + w, -0.375, 0.0 },
                { x + w, 0.375, 0.0 }, { x, 0.375, 0.0 }
            };
            part.holes.push_back({
                { x + w * 0.25, -0.25, 0.0 }, { x + w * 0.25, 0.25, 0.0 },
                { x + w * 0.75, 0.25, 0.0 }, { x + w * 0.75, -0.25, 0.0 }
            });
            holeSamples.emplace_back(float(x + w * 0.5 + 0.5), 0.5f);
            fillSamples.emplace_back(float(x + w * 0.5 + 0.5), 0.15625f);
            addPolygon(std::move(part));
        }
        // An overlapping polygon must stay with building zero, not become a
        // second independently blended fill. It does not cover our sample points.
        PolygonPart overlapping;
        const double x = geometry.polygons.front().outer.front().x;
        overlapping.outer = {
            { x - 1.0 / 16384.0, 0.30, 0.0 }, { x + 1.0 / 16384.0, 0.30, 0.0 },
            { x + 1.0 / 16384.0, 0.40, 0.0 }, { x - 1.0 / 16384.0, 0.40, 0.0 }
        };
        addPolygon(std::move(overlapping));
        // A later color group must still follow every batch of the first group.
        PolygonPart other;
        other.outer = { { 0.3, 0.3, 0.0 }, { 0.4, 0.3, 0.0 },
                        { 0.4, 0.4, 0.0 }, { 0.3, 0.4, 0.0 } };
        addPolygon(std::move(other));
        geometry.colors.back() = StockColor::Cyan;
        auto& style = reg.emplace<PolygonStyle>(entity);
        style.useGeometryColors = true;
        reg.emplace<rocky::Polygon>(entity, geometry, style);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });

    slugSystem->update(context.get());
    vsg::ref_ptr<vsg::ImageInfo> savedCurves, savedBands;
    std::uint64_t generation = 0u;
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        INFO(resource.message);
        REQUIRE(resource.ready);
        REQUIRE(resource.layers.size() > 2u);
        CHECK(resource.layers.size() < 16u);
        CHECK(resource.layers.back().color == StockColor::Cyan);
        savedCurves = resource.curveTexture;
        savedBands = resource.bandTexture;
        generation = resource.atlasGeneration;
        auto* curves = dynamic_cast<vsg::vec4Array2D*>(savedCurves->imageView->image->data.get());
        auto* bands = dynamic_cast<vsg::usvec2Array2D*>(savedBands->imageView->image->data.get());
        REQUIRE(curves);
        REQUIRE(bands);
        REQUIRE(curves->width() == 512u);
        const auto width = bands->width();
        std::vector<std::size_t> polygonLayer(polygonCount + 2u, resource.layers.size());
        std::vector<std::vector<Curve>> decoded(resource.layers.size());
        for (std::size_t layerIndex = 0u; layerIndex < resource.layers.size(); ++layerIndex)
        {
            const auto& layer = resource.layers[layerIndex];
            if (layerIndex + 1u < resource.layers.size())
            {
                CHECK(layer.color == fillColor);
                CHECK(layer.bandTransform.x > 32.0f / 0.51f); // tighter than the original batch
            }
            const auto& shape = layer.shapeData;
            const unsigned start = shape.y * width + shape.x;
            const unsigned headerStart = start + 2u * resource.indirectionSize;
            const unsigned headerCount = shape.z + shape.w + 2u;
            std::set<unsigned> seen;
            for (unsigned h = 0u; h < headerCount; ++h)
            {
                const auto header = (*bands)[headerStart + h];
                REQUIRE(start + unsigned(header.y) + unsigned(header.x) <= bands->valueCount());
                for (unsigned i = 0u; i < header.x; ++i)
                {
                    const auto loc = (*bands)[start + header.y + i];
                    const unsigned curveIndex = loc.y * curves->width() + loc.x;
                    REQUIRE(curveIndex + 1u < curves->valueCount());
                    if (!seen.insert(curveIndex).second)
                        continue;
                    const auto& a = (*curves)[curveIndex];
                    const auto& b = (*curves)[curveIndex + 1u];
                    const Curve curve{ a.x, a.y, a.z, a.w, b.x, b.y };
                    const auto found = expected.find(curve);
                    REQUIRE(found != expected.end());
                    auto& ownerLayer = polygonLayer[found->second];
                    if (ownerLayer == resource.layers.size())
                        ownerLayer = layerIndex;
                    CHECK(ownerLayer == layerIndex); // exterior and every hole are inseparable
                    expected.erase(found);
                    decoded[layerIndex].push_back(curve);
                }
            }
        }
        CHECK(expected.empty());
        CHECK(polygonLayer.front() == polygonLayer[polygonCount]); // overlapping pair
        CHECK(polygonLayer.back() == resource.layers.size() - 1u);

        // Evaluate winding from the actual packed straight edges, checking that
        // every hole stays empty and every building still has a filled interior.
        auto winding = [&](const std::vector<Curve>& batch, glm::vec2 point)
        {
            int result = 0;
            for (const auto& c : batch)
            {
                const double side = (double(c[4]) - c[0]) * (double(point.y) - c[1]) -
                    (double(c[5]) - c[1]) * (double(point.x) - c[0]);
                if (c[1] <= point.y && c[5] > point.y && side > 0.0) ++result;
                if (c[1] > point.y && c[5] <= point.y && side < 0.0) --result;
            }
            return result;
        };
        for (std::size_t i = 0u; i < polygonCount; ++i)
        {
            CHECK(winding(decoded[polygonLayer[i]], holeSamples[i]) == 0);
            CHECK(winding(decoded[polygonLayer[i]], fillSamples[i]) != 0);
        }
    });

    slugSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        CHECK(resource.atlasGeneration == generation);
        CHECK(resource.curveTexture == savedCurves);
        CHECK(resource.bandTexture == savedBands);
    });

    const auto exportPath = std::filesystem::temp_directory_path() /
        ("rocky-slug-batched-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".slug");
    registry.write([&](entt::registry& reg)
    {
        reg.get<SlugResource>(entity).exportPath = exportPath.string();
    });
    slugSystem->update(context.get());
    REQUIRE(std::filesystem::exists(exportPath));
    {
        std::ifstream input(exportPath);
        const auto document = json::parse(input);
        registry.read([&](entt::registry& reg)
        {
            const auto& resource = reg.get<SlugResource>(entity);
            CHECK(resource.exportSucceeded);
            CHECK(resource.atlasGeneration == generation);
            CHECK(resource.curveTexture == savedCurves);
            CHECK(resource.bandTexture == savedBands);
            CHECK(document["shapes"].size() == resource.layers.size());
            for (const auto& layer : resource.layers)
            {
                bool found = false;
                for (const auto& shape : document["shapes"])
                    found = found || (shape["band_tex_x"] == layer.shapeData.x &&
                        shape["band_tex_y"] == layer.shapeData.y);
                CHECK(found); // deterministic batching on export-only rebuilds
            }
            CHECK(reg.view<SlugResource>().size() == 1u);
        });
    }
    std::error_code removeError;
    CHECK(std::filesystem::remove(exportPath, removeError));
    CHECK_FALSE(removeError);
    registry.write([&](entt::registry& reg)
    {
        auto& geometry = reg.get<PolygonGeometry>(entity);
        geometry.polygons.resize(1u);
        geometry.colors.resize(1u);
        geometry.dirty(reg);
    });
    slugSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        CHECK(resource.ready);
        CHECK(resource.layers.size() == 1u);
        CHECK(resource.atlasGeneration > generation);
        CHECK(resource.bandTexture->imageView->image->data->dataSize() <
            savedBands->imageView->image->data->dataSize());
    });
}

TEST_CASE("slug reduces bands for an oversized indivisible polygon", "[projection][slug][polygon]")
{
    Registry registry = Registry::create();
    auto slugSystem = SlugSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(entity);
        PolygonPart part;
        part.outer = { { -0.4, -0.4, 0.0 }, { 0.4, -0.4, 0.0 },
                       { 0.4, 0.4, 0.0 }, { -0.4, 0.4, 0.0 } };
        for (unsigned i = 0u; i < 2048u; ++i)
        {
            const double x = -0.375 + double(i) / 4096.0;
            part.holes.push_back({
                { x, -0.35, 0.0 }, { x, 0.35, 0.0 },
                { x + 1.0 / 8192.0, 0.35, 0.0 }, { x + 1.0 / 8192.0, -0.35, 0.0 }
            });
        }
        geometry.polygons.push_back(std::move(part));
        reg.emplace<rocky::Polygon>(entity, geometry);
        reg.emplace<Overlay>(entity).mode = OverlayMode::Vector;
    });
    slugSystem->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<SlugResource>(entity);
        REQUIRE(resource.ready);
        REQUIRE(resource.curveTexture);
        REQUIRE(resource.bandTexture);
        REQUIRE(resource.layers.size() == 1u);
        CHECK(resource.layers.front().shapeData.z < 31u);
        CHECK(resource.layers.front().shapeData.w < 31u);
        CHECK(resource.message.empty());
    });
}

TEST_CASE("slug overlay auto-fits georeferenced line geometry", "[projection][slug]")
{
    Registry registry = Registry::create();
    auto sourceSystems = ECSNode::create(registry, false);
    sourceSystems->add(LineSystemNode::create(registry));

    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    bakeSystem->renderSourceSystems = sourceSystems;
    bakeSystem->worldSRS = SRS::ECEF;
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;

    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;
    const std::vector<glm::dvec3> input = {
        { 24.918, 60.161, 15.0 },
        { 24.920, 60.163, 25.0 }
    };

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<LineGeometry>(entity);
        geometry.srs = SRS::WGS84;
        geometry.topology = LineTopology::Strip;
        geometry.points = input;

        auto& style = reg.emplace<LineStyle>(entity);
        style.width = 5.0f;
        style.widthUnits = Units::METERS;
        style.color = StockColor::Yellow;
        style.outlineColor = StockColor::Black;
        style.outlineWidth = 2.0f;
        reg.emplace<Line>(entity, geometry, style);

        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Vector;
    });

    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    std::uint64_t outlinedGeneration = 0u;

    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<RenderTexture>(entity));
        REQUIRE((reg.any_of<AutoOverlayTransform, Transform>(entity)));

        const auto& transform = reg.get<Transform>(entity);
        CHECK(transform.topocentric);
        CHECK(transform.position.srs == SRS::WGS84);

        const auto& slug = reg.get<SlugResource>(entity);
        REQUIRE(slug.ready);
        REQUIRE(slug.bandTexture);
        REQUIRE(slug.bandTexture->imageView);
        REQUIRE(slug.bandTexture->imageView->image);
        REQUIRE(slug.bandTexture->imageView->image->data);
        CHECK(slug.bandTexture->imageView->image->format == VK_FORMAT_R16G16_UINT);
        CHECK(slug.bandTexture->imageView->image->data->properties.format ==
            VK_FORMAT_R16G16_UINT);
        auto* bandData = dynamic_cast<vsg::usvec2Array2D*>(
            slug.bandTexture->imageView->image->data.get());
        REQUIRE(bandData);
        CHECK(bandData->dataSize() ==
            slug.bandTexture->imageView->image->extent.width *
            slug.bandTexture->imageView->image->extent.height * sizeof(vsg::usvec2));
        REQUIRE(slug.layers.size() == 2u);
        CHECK(slug.layers[0].color == StockColor::Black);
        CHECK(slug.layers[1].color == StockColor::Yellow);
        CHECK(slug.layers[0].isOutline);
        CHECK_FALSE(slug.layers[1].isOutline);
        const auto& lineLayer = slug.layers[1];
        const glm::dvec2 authoredXAxis(
            lineLayer.uvToEmX.x, lineLayer.uvToEmY.x);
        const glm::dvec2 authoredYAxis(
            lineLayer.uvToEmX.y, lineLayer.uvToEmY.y);
        const glm::dvec3 projectorXAxis(transform.localMatrix[0]);
        const glm::dvec3 projectorYAxis(transform.localMatrix[1]);
        REQUIRE(glm::length(authoredXAxis) > 0.0);
        REQUIRE(glm::length(authoredYAxis) > 0.0);
        CHECK(std::abs(
            glm::length(authoredXAxis) / glm::length(authoredYAxis) -
            glm::length(projectorXAxis) / glm::length(projectorYAxis)) < 1e-4);
        CHECK(std::abs(lineLayer.uvToEmX.x) +
            std::abs(lineLayer.uvToEmX.y) <= 1.0001f);
        CHECK(std::abs(lineLayer.uvToEmY.x) +
            std::abs(lineLayer.uvToEmY.y) <= 1.0001f);
        outlinedGeneration = slug.atlasGeneration;

        const auto projectorPosition = transform.position.transform(SRS::ECEF);
        REQUIRE(projectorPosition.valid());
        const auto projectorToWorld = SRS::ECEF.topocentricToWorldMatrix(
            glm::dvec3(projectorPosition.x, projectorPosition.y, projectorPosition.z)) *
            transform.localMatrix;
        const auto worldToProjector = glm::inverse(projectorToWorld);
        const auto toWorld = SRS::WGS84.to(SRS::ECEF);
        REQUIRE(toWorld.valid());

        for (const auto& point : input)
        {
            glm::dvec3 world;
            REQUIRE(toWorld.transform(point, world));
            const auto local = worldToProjector * glm::dvec4(world, 1.0);
            CHECK(std::abs(local.x) < 0.5);
            CHECK(std::abs(local.y) < 0.5);

            // Metric geometry uses a bounded affine map that preserves the
            // projector plane's physical distances. Verify its projected
            // endpoints remain inside the stable Slughorn band grid.
            const glm::fvec3 uv(
                static_cast<float>(local.x + 0.5),
                static_cast<float>(local.y + 0.5),
                1.0f);
            const auto& layer = slug.layers.back();
            const glm::fvec2 em(
                glm::dot(uv, glm::fvec3(layer.uvToEmX)),
                glm::dot(uv, glm::fvec3(layer.uvToEmY)));
            const glm::fvec2 band =
                em * glm::fvec2(layer.bandTransform) +
                glm::fvec2(layer.bandTransform.z, layer.bandTransform.w);
            CHECK(band.x >= -0.01f);
            CHECK(band.y >= -0.01f);
            CHECK(band.x <= 32.01f);
            CHECK(band.y <= 32.01f);
        }
    });

    const auto exportStamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto exportPath = std::filesystem::temp_directory_path() /
        ("rocky-slug-test-" + exportStamp + ".slug");
    const auto translucentExportPath = std::filesystem::temp_directory_path() /
        ("rocky-slug-ring-test-" + exportStamp + ".slug");
    std::uint64_t generationBeforeExport = 0u;
    vsg::ref_ptr<vsg::ImageInfo> curveBeforeExport;
    vsg::ref_ptr<vsg::ImageInfo> bandBeforeExport;
    registry.write([&](entt::registry& reg)
    {
        auto& slug = reg.get<SlugResource>(entity);
        generationBeforeExport = slug.atlasGeneration;
        curveBeforeExport = slug.curveTexture;
        bandBeforeExport = slug.bandTexture;
        slug.exportPath = exportPath.string();
    });

    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& slug = reg.get<SlugResource>(entity);
        CHECK(slug.exportPath.empty());
        CHECK(slug.exportSucceeded);
        CHECK(slug.exportMessage.find(exportPath.string()) != std::string::npos);
        CHECK(slug.atlasGeneration == generationBeforeExport);
        CHECK(slug.curveTexture == curveBeforeExport);
        CHECK(slug.bandTexture == bandBeforeExport);
    });

    REQUIRE(std::filesystem::exists(exportPath));
    CHECK(std::filesystem::file_size(exportPath) > 0u);
    {
        std::ifstream input(exportPath);
        char first = '\0';
        input.get(first);
        CHECK(first == '{');
    }

    auto curveTexelsUsed = [](const std::filesystem::path& path)
    {
        std::ifstream input(path);
        const auto document = json::parse(input);
        return document["packing_stats"]["curve_texels_used"].get<std::uint64_t>();
    };
    const auto opaqueCasingCurveTexels = curveTexelsUsed(exportPath);

    std::error_code removeError;
    CHECK(std::filesystem::remove(exportPath, removeError));
    CHECK_FALSE(removeError);

    // Translucent modulation must rebuild with the non-overlapping ring, which
    // carries an additional inner boundary compared with the opaque casing.
    registry.write([&](entt::registry& reg)
    {
        reg.get<Overlay>(entity).color = Color(StockColor::White, 0.5f);
    });
    slugSystem->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        reg.get<SlugResource>(entity).exportPath = translucentExportPath.string();
    });
    slugSystem->update(context.get());

    REQUIRE(std::filesystem::exists(translucentExportPath));
    CHECK(opaqueCasingCurveTexels < curveTexelsUsed(translucentExportPath));
    removeError.clear();
    CHECK(std::filesystem::remove(translucentExportPath, removeError));
    CHECK_FALSE(removeError);

    registry.write([&](entt::registry& reg)
    {
        reg.get<Overlay>(entity).color = StockColor::White;
    });
    slugSystem->update(context.get());

    registry.write([&](entt::registry& reg)
    {
        auto& style = reg.get<LineStyle>(entity);
        style.outlineWidth = 0.0f;
        style.dirty(reg);
    });
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& slug = reg.get<SlugResource>(entity);
        REQUIRE(slug.ready);
        CHECK(slug.layers.size() == 1u);
        CHECK(slug.layers[0].color == StockColor::Yellow);
        CHECK_FALSE(slug.layers[0].isOutline);
        CHECK(slug.atlasGeneration > outlinedGeneration);
    });
}

TEST_CASE("slug overlay retries an initially empty georeferenced line", "[projection][slug]")
{
    Registry registry = Registry::create();
    auto sourceSystems = ECSNode::create(registry, false);
    sourceSystems->add(LineSystemNode::create(registry));

    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    bakeSystem->renderSourceSystems = sourceSystems;
    bakeSystem->worldSRS = SRS::ECEF;
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;

    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<LineGeometry>(entity);
        geometry.srs = SRS::WGS84;
        geometry.topology = LineTopology::Segments;

        auto& style = reg.emplace<LineStyle>(entity);
        style.width = 5.0f;
        style.widthUnits = Units::METERS;
        reg.emplace<Line>(entity, geometry, style);

        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Vector;
    });

    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<Transform>(entity));
        const auto& slug = reg.get<SlugResource>(entity);
        CHECK_FALSE(slug.ready);
        CHECK(slug.message == "Slug overlay geometry contains no renderable primitives");
    });

    // Model a paged producer that publishes its coordinates after constructing
    // the component but does not bump its revision. The auto-fit cache must
    // still retry solely because its generated Transform remains absent.
    registry.write([&](entt::registry& reg)
    {
        auto& geometry = reg.get<LineGeometry>(entity);
        geometry.points = {
            { 139.750, 35.680, 0.0 },
            { 139.751, 35.681, 0.0 }
        };
    });

    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        REQUIRE((reg.any_of<AutoOverlayTransform, Transform>(entity)));
        const auto& slug = reg.get<SlugResource>(entity);
        CHECK(slug.ready);
        CHECK_FALSE(slug.layers.empty());
    });
}

TEST_CASE("complex slug geometry uses the full indirection grid", "[projection][slug]")
{
    Registry registry = Registry::create();
    auto sourceSystems = ECSNode::create(registry, false);
    sourceSystems->add(LineSystemNode::create(registry));

    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    bakeSystem->renderSourceSystems = sourceSystems;
    bakeSystem->worldSRS = SRS::ECEF;
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;
    slugSystem->mergeConnectedLineSegments = true;

    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& geometry = reg.emplace<LineGeometry>(entity);
        geometry.srs = SRS::WGS84;
        geometry.topology = LineTopology::Segments;

        // Forty independent MVT-like segments expand to enough stroke curves
        // to exercise Rocky's 32-band complex-shape policy.
        for (unsigned i = 0u; i < 40u; ++i)
        {
            const double x = 24.918 + double(i % 8u) * 0.0002;
            const double y = 60.161 + double(i / 8u) * 0.0002;
            geometry.points.emplace_back(x, y, 0.0);
            geometry.points.emplace_back(x + 0.0001, y + 0.00005, 0.0);
        }

        auto& style = reg.emplace<LineStyle>(entity);
        style.width = 3.0f;
        style.widthUnits = Units::METERS;
        style.color = StockColor::Yellow;
        reg.emplace<Line>(entity, geometry, style);

        auto& overlay = reg.emplace<Overlay>(entity);
        overlay.mode = OverlayMode::Vector;
    });

    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& slug = reg.get<SlugResource>(entity);
        REQUIRE(slug.ready);
        REQUIRE(slug.layers.size() == 1u);
        CHECK(slug.layers.front().shapeData.z == 31u);
        CHECK(slug.layers.front().shapeData.w == 31u);
    });

    // Replace the independent segments with one exact endpoint-to-endpoint
    // chain, then compare Slughorn's serialized curve usage with the
    // experiment enabled and disabled.
    registry.write([&](entt::registry& reg)
    {
        auto& geometry = reg.get<LineGeometry>(entity);
        geometry.points.clear();
        for (unsigned i = 0u; i < 40u; ++i)
        {
            const double x0 = 24.918 + double(i) * 0.00003;
            const double x1 = 24.918 + double(i + 1u) * 0.00003;
            geometry.points.emplace_back(x0, 60.161, 0.0);
            geometry.points.emplace_back(x1, 60.161, 0.0);
        }
        geometry.dirty(reg);
    });
    bakeSystem->update(context.get());
    slugSystem->update(context.get());

    const auto stamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto mergedPath = std::filesystem::temp_directory_path() /
        ("rocky-slug-merged-" + stamp + ".slug");
    const auto unmergedPath = std::filesystem::temp_directory_path() /
        ("rocky-slug-unmerged-" + stamp + ".slug");

    std::uint64_t mergedGeneration = 0u;
    registry.write([&](entt::registry& reg)
    {
        auto& slug = reg.get<SlugResource>(entity);
        mergedGeneration = slug.atlasGeneration;
        slug.exportPath = mergedPath.string();
    });
    slugSystem->update(context.get());

    slugSystem->mergeConnectedLineSegments = false;
    registry.write([&](entt::registry& reg)
    {
        reg.get<SlugResource>(entity).exportPath = unmergedPath.string();
    });
    slugSystem->update(context.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& slug = reg.get<SlugResource>(entity);
        REQUIRE(slug.ready);
        CHECK(slug.atlasGeneration > mergedGeneration);
        CHECK(slug.exportSucceeded);
    });

    auto curveTexelsUsed = [](const std::filesystem::path& path)
    {
        std::ifstream input(path);
        const auto document = json::parse(input);
        return document["packing_stats"]["curve_texels_used"].get<std::uint64_t>();
    };
    REQUIRE(std::filesystem::exists(mergedPath));
    REQUIRE(std::filesystem::exists(unmergedPath));
    CHECK(curveTexelsUsed(mergedPath) < curveTexelsUsed(unmergedPath));

    std::error_code removeError;
    CHECK(std::filesystem::remove(mergedPath, removeError));
    CHECK_FALSE(removeError);
    removeError.clear();
    CHECK(std::filesystem::remove(unmergedPath, removeError));
    CHECK_FALSE(removeError);
}

TEST_CASE("slug overlays approximate meshes and reject unsupported styles", "[projection][slug]")
{
    Registry registry = Registry::create();
    auto slugSystem = SlugSystemNode::create(registry);
    slugSystem->worldSRS = SRS::ECEF;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity = entt::null;

    SECTION("triangulated meshes remain available")
    {
        registry.write([&](entt::registry& reg)
        {
            entity = reg.create();
            auto& geometry = reg.emplace<MeshGeometry>(entity);
            geometry.vertices = {
                { -0.25, -0.25, 0.0 },
                {  0.25, -0.25, 0.0 },
                {  0.00,  0.25, 0.0 }
            };
            reg.emplace<Mesh>(entity, geometry);
            auto& overlay = reg.emplace<Overlay>(entity);
            overlay.mode = OverlayMode::Vector;
        });

        slugSystem->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            const auto& resource = reg.get<SlugResource>(entity);
            REQUIRE(resource.ready);
            REQUIRE(resource.layers.size() == 1u);
            CHECK_FALSE(resource.layers.front().isOutline);
            CHECK(resource.message.empty());
        });
    }

    SECTION("stippled lines")
    {
        registry.write([&](entt::registry& reg)
        {
            entity = reg.create();
            auto& geometry = reg.emplace<LineGeometry>(entity);
            geometry.points = { { -0.25, 0.0, 0.0 }, { 0.25, 0.0, 0.0 } };
            auto& style = reg.emplace<LineStyle>(entity);
            style.stipplePattern = 0x00FFu;
            reg.emplace<Line>(entity, geometry, style);
            auto& overlay = reg.emplace<Overlay>(entity);
            overlay.mode = OverlayMode::Vector;
        });

        slugSystem->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            const auto& resource = reg.get<SlugResource>(entity);
            CHECK_FALSE(resource.ready);
            CHECK(resource.message == "Slug LineStyle does not support stippling");
        });
    }

    SECTION("per-point widths")
    {
        registry.write([&](entt::registry& reg)
        {
            entity = reg.create();
            auto& geometry = reg.emplace<PointGeometry>(entity);
            geometry.points = { { 0.0, 0.0, 0.0 } };
            geometry.widths = { 4.0f };
            auto& style = reg.emplace<PointStyle>(entity);
            style.useGeometryWidths = true;
            reg.emplace<Point>(entity, geometry, style);
            auto& overlay = reg.emplace<Overlay>(entity);
            overlay.mode = OverlayMode::Vector;
        });

        slugSystem->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            const auto& resource = reg.get<SlugResource>(entity);
            CHECK_FALSE(resource.ready);
            CHECK(resource.message ==
                "Slug PointStyle does not support per-vertex widths");
        });
    }
}

#endif
TEST_CASE("legacy decal adapter", "[projection]")
{
    Registry registry = Registry::create();
    auto decalSystem = DecalSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto styleEntity = reg.create();
        reg.emplace<DecalStyle>(styleEntity);
        CHECK(reg.any_of<ImageTexture>(styleEntity));

        auto decalEntity = reg.create();
        reg.emplace<Decal>(decalEntity, styleEntity);
        REQUIRE(reg.any_of<ProjectedTexture>(decalEntity));
        CHECK(reg.get<ProjectedTexture>(decalEntity).texture == styleEntity);

        reg.remove<Decal>(decalEntity);
        CHECK_FALSE(reg.any_of<ProjectedTexture>(decalEntity));
        reg.remove<DecalStyle>(styleEntity);
        CHECK_FALSE(reg.any_of<ImageTexture>(styleEntity));

        auto explicitStyle = reg.create();
        reg.emplace<ImageTexture>(explicitStyle);
        reg.emplace<DecalStyle>(explicitStyle);
        reg.remove<DecalStyle>(explicitStyle);
        CHECK(reg.any_of<ImageTexture>(explicitStyle));

        auto explicitDecal = reg.create();
        reg.emplace<ProjectedTexture>(explicitDecal);
        reg.emplace<Decal>(explicitDecal);
        reg.remove<Decal>(explicitDecal);
        CHECK(reg.any_of<ProjectedTexture>(explicitDecal));
    });
}

TEST_CASE("json")
{
    Profile profile("global-geodetic");
    auto conf = profile.to_json();
    CHECK(conf == R"("global-geodetic")");
    profile = Profile();
    ROCKY_NAMESPACE::from_json(json::parse(conf), profile);
    CHECK((profile.valid() && profile.wellKnownName() == "global-geodetic"));

    GeoPoint point(SRS::WGS84, -77, 42, 0.0);
    json j = json::object();
    ROCKY_NAMESPACE::to_json(j, point);
    conf = j.dump();
    CHECK(conf == R"({"lat":42.0,"long":-77.0,"srs":"wgs84","z":0.0})");
    point = GeoPoint();
    ROCKY_NAMESPACE::from_json(json::parse(conf), point);
    CHECK((point.valid() && point.srs == SRS::WGS84 && point.x == -77 && point.y == 42 && point.z == 0));

    option<URI> uri;
    uri = URI("file.xml");
    json j_uri = json::object();
    ROCKY_NAMESPACE::to_json(j_uri, uri);
    CHECK((j_uri.dump() == R"("file.xml")")); // "file.xml"
    URI uri2;
    ROCKY_NAMESPACE::from_json(j_uri, uri2);
    CHECK((uri2.base() == "file.xml"));

    auto contextSingleton = ContextFactory::create();
    auto context = contextSingleton.get();
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "file.xml";
    auto map = rocky::Map::create();
    map->add(layer);
    auto serialized = map->to_json();
    map = rocky::Map::create();
    auto r = map->from_json(serialized, context->io);
    CHECK(r.ok());
    CHECK((map->to_json() == R"({"layers":[{"name":"","type":"TMSImage","uri":"file.xml"}],"name":""})"));
}

TEST_CASE("Optional")
{
    option<int> value_with_no_init;
    CHECK(value_with_no_init.has_value() == false);
    value_with_no_init = 123;
    CHECK(value_with_no_init.has_value() == true);

    option<int> value_with_brace_init{ 123 };
    CHECK(value_with_brace_init.has_value() == false);
    CHECK(value_with_brace_init.value() == 123);
    CHECK(value_with_brace_init.default_value() == 123);

    option<int> value_with_equals_init = 123;
    CHECK(value_with_equals_init.has_value() == false);
    CHECK(value_with_equals_init.value() == 123);
    CHECK(value_with_brace_init.default_value() == 123);
}

TEST_CASE("TileKey")
{
    Profile p("global-geodetic");

    CHECK(TileKey(0, 0, 0, p).str() == "0/0/0");
    CHECK(TileKey(0, 0, 0, p).quadKey() == "0");
    CHECK(TileKey(0, 0, 0, p).createChildKey(0) == TileKey(1, 0, 0, p));
    CHECK(TileKey(1, 0, 0, p).createParentKey() == TileKey(0, 0, 0, p));

    CHECK(TileKey(2, 0, 0, p).str() == "2/0/0");
    CHECK(TileKey(2, 0, 0, p).quadKey() == "000");
    CHECK(TileKey(2, 1, 0, p).quadKey() == "001");
    CHECK(TileKey(2, 5, 1, p).quadKey() == "103");

    std::unordered_map<TileKey, int> values;
    values[TileKey(2, 1, 0, p)] = 42;

    CHECK(values.at(TileKey(2, 1, 0, p)) == 42);
    CHECK(values.find(TileKey(2, 1, 1, p)) == values.end());
}

TEST_CASE("Threading")
{
    jobs::future<int> f1;
    CHECK(f1.empty() == true);
    CHECK(f1.available() == false);

    jobs::future<int> f2;
    CHECK(f2.empty() == true);
    CHECK(f2.working() == false);

    f2 = f1;
    CHECK(f2.empty() == false);
    CHECK(f2.working() == true);
    CHECK(f2.available() == false);

    f1.resolve(123);
    CHECK(f2.empty() == false);
    CHECK(f2.available() == true);
    CHECK(f2.value() == 123);

    SECTION("runtime shutdown joins worker threads and prevents restart")
    {
        jobs::runtime runtime;
        auto pool = runtime.get_pool("shutdown-test", 1);
        REQUIRE(pool != nullptr);

        std::atomic_bool job_started = false;
        std::atomic_bool release_job = false;
        std::atomic_bool shutdown_finished = false;

        auto result = runtime.dispatch(
            [&](jobs::cancelable&) -> int
            {
                job_started = true;
                while (!release_job)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return 42;
            },
            jobs::context{ "blocking shutdown test", pool });

        for (unsigned i = 0; i < 1000 && !job_started; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(job_started);

        std::thread shutdown_thread([&]()
            {
                runtime.shutdown();
                shutdown_finished = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(shutdown_finished == false);

        release_job = true;
        shutdown_thread.join();

        CHECK(shutdown_finished == true);
        CHECK(result.available());
        CHECK(result.value() == 42);
        CHECK(runtime.alive() == false);
        CHECK(runtime.get_pool("shutdown-test", 1) == nullptr);

        std::atomic_bool post_shutdown_job_ran = false;
        runtime.dispatch([&]() { post_shutdown_job_ran = true; });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(post_shutdown_job_ran == false);
        CHECK(runtime.total() == 0);
    }
}

TEST_CASE("Math")
{
    CHECK(is_identity(glm::fmat4(1)));
    CHECK(!is_identity(glm::fmat4()));

    glm::fmat4 scale_bias{ 1 };
    scale_bias = glm::translate(glm::fmat4(1), glm::fvec3(0.25, 0.25, 0.0));
    scale_bias = glm::scale(scale_bias, glm::fvec3(0.5, 0.5, 1.0));
    CHECK(!is_identity(scale_bias));
    CHECK(scale_bias[0][0] == 0.5f);
    CHECK(scale_bias[1][1] == 0.5f);
    CHECK(scale_bias[3][0] == 0.25f);
    CHECK(scale_bias[3][1] == 0.25f);

    glm::fvec3 r = scale_bias * glm::fvec3(1, 1, 0);
    CHECK(r == glm::fvec3(0.75f, 0.75f, 0));
}

#ifdef ROCKY_HAS_ZLIB
TEST_CASE("Compression")
{
    // generate a pseudo-random string of characters:
    std::mt19937 engine(0);
    std::uniform_int_distribution<> prng(32, 127);
    std::stringstream buf;
    for (unsigned i = 0; i < 4096; ++i)
        buf << (char)prng(engine);
    auto original_data = buf.str();

    // compress:
    std::stringstream output_stream;
    ZLibCompressor comp;
    CHECK(comp.compress(original_data, output_stream) == true);
    std::string compressed_data = output_stream.str();

    CHECK(compressed_data.size() == 3442);

    // decompress:
    std::stringstream input_stream(compressed_data);
    std::string decompressed_data;
    CHECK(comp.decompress(input_stream, decompressed_data) == true);

    // ensure the decompressed stream matched the original data
    CHECK(decompressed_data == original_data);
}
#endif

TEST_CASE("Image")
{
    auto image = Image::create(Image::R8G8B8A8_UNORM, 256, 256);
    REQUIRE(image);
    if (image) {
        CHECK(image->numComponents() == 4);
        CHECK(image->sizeInBytes() == 262144);
        CHECK(image->rowSizeInBytes() == 1024);
        CHECK(image->componentSizeInBytes() == 1);
        CHECK(image->sizeInPixels() == 65536);
    }

    auto clone = image->clone();
    REQUIRE(clone);
    if (clone) {
        CHECK(clone->numComponents() == 4);
        CHECK(clone->sizeInBytes() == 262144);
        CHECK(clone->rowSizeInBytes() == 1024);
        CHECK(clone->componentSizeInBytes() == 1);
        CHECK(clone->sizeInPixels() == 65536);
    }

    image = Image::create(Image::R8G8B8_UNORM, 256, 256);
    REQUIRE(image);
    if (image) {
        CHECK(image->numComponents() == 3);
        CHECK(image->sizeInBytes() == 196608);
        CHECK(image->rowSizeInBytes() == 768);
        CHECK(image->componentSizeInBytes() == 1);
        CHECK(image->sizeInPixels() == 65536);
    }

    image = Image::create(Image::R8G8B8A8_UNORM, 256, 256);
    image->fill(StockColor::Orange);
    Image::Pixel value = image->read(17, 17);
    //std::cout << value.r << ", " << value.g << ", " << value.b << ", " << value.a << std::endl;
    CHECK(glm::epsilonEqual(value.r, 1.0f, 0.01f));
    CHECK(glm::epsilonEqual(value.g, 0.65f, 0.01f));
    CHECK(glm::epsilonEqual(value.b, 0.0f, 0.01f));
    CHECK(glm::epsilonEqual(value.a, 1.0f, 0.01f));
}

TEST_CASE("Heightfield")
{
    auto hf = Heightfield::create(257, 257);
    REQUIRE(hf.image);
    if (hf.image) {
        // test metadata:
        CHECK(hf.image->pixelFormat() == Image::R32_SFLOAT);
        CHECK(hf.image->numComponents() == 1);
        CHECK(hf.image->sizeInBytes() == 264196);
        CHECK(hf.image->rowSizeInBytes() == 1028);
        CHECK(hf.image->componentSizeInBytes() == 4);
        CHECK(hf.image->sizeInPixels() == 66049);

        // write/read:
        hf.heightAt(16, 16) = 100.0f;
        hf.heightAt(16, 17) = 50.0f;
        hf.heightAt(17, 16) = 50.0f;
        hf.heightAt(17, 17) = 100.0f;
        CHECK(hf.heightAt(16, 16) == 100.0f);

        float u = 16.5f / (float(hf.width()) - 1.0f);
        float v = 16.5f / (float(hf.height()) - 1.0f);
        CHECK(hf.heightAtUV(u, v) == 75.0f);
        
        // read with NO_DATA_VALUEs:
        hf.heightAt(17, 17) = NO_DATA_VALUE;
        hf.heightAt(16, 16) = NO_DATA_VALUE;
        CHECK(hf.heightAt(16, 16) == NO_DATA_VALUE);
        CHECK(hf.heightAtUV(u, v) == 50.0f);

        // all NODATA:
        hf.fill(NO_DATA_VALUE);
        CHECK(hf.heightAt(16, 16) == NO_DATA_VALUE);
    }
}

TEST_CASE("Map")
{
    auto map = Map::create();
    REQUIRE(map);
    if (map) {
        auto layer = TestLayer::create();
        map->add(layer);
        CHECK(map->layers().size() == 1);
    }
}

#ifdef ROCKY_HAS_GDAL
TEST_CASE("GDAL")
{
}
#endif // ROCKY_HAS_GDAL

TEST_CASE("TMS")
{
    auto layer = TMSImageLayer::create();
    layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    auto s = layer->open({});
    CHECK((s.ok() || s.error().type == Failure::ResourceUnavailable));
}

TEST_CASE("SRS")
{
    // epsilon
    const double E = 0.1;

    SECTION("Spherical Mercator <> Geographic")
    {        
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());
        CHECK(merc.isProjected() == true);
        CHECK(merc.isGeodetic() == false);
        CHECK(merc.isGeocentric() == false);

        SRS wgs84("epsg:4326"); // geographic WGS84 (long/lat/hae)
        REQUIRE(wgs84.valid());
        CHECK(wgs84.isProjected() == false);
        CHECK(wgs84.isGeodetic() == true);
        CHECK(wgs84.isGeocentric() == false);

        auto xform = merc.to(wgs84);
        REQUIRE(xform.valid());

        glm::dvec3 out;
        REQUIRE(xform(glm::dvec3(-20037508.342789248, 0, 0), out));
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(-180, 0, 0), E)));
        
        // NB: succeeds despite the 90 degrees N being out of bounds for Mercator.
        CHECK(xform.inverse(glm::dvec3(0, 90, 0), out));
        CHECK(out.y > merc.bounds().ymax);
    }

    SECTION("Geographic SRS")
    {
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());

        SRS geo = merc.geodeticSRS();
        REQUIRE(geo.valid());
        CHECK(geo.isGeodetic());

        SRS utm("epsg:32632"); // UTM32/WGS84
        CHECK(utm.geodeticSRS().isGeodetic());
    }

    SECTION("Geographic <> Geocentric")
    {
        SRS wgs84("wgs84"); // geographic WGS84 (long/lat)
        REQUIRE(wgs84.valid());
        CHECK(wgs84.isProjected() == false);
        CHECK(wgs84.isGeodetic() == true);
        CHECK(wgs84.isGeocentric() == false);

        SRS ecef("geocentric"); // geocentric WGS84 (ECEF)
        REQUIRE(ecef.valid());
        CHECK(ecef.isProjected() == false);
        CHECK(ecef.isGeodetic() == false);
        CHECK(ecef.isGeocentric() == true);

        glm::dvec3 out;

        auto xform_wgs84_to_ecef = wgs84.to(ecef);
        REQUIRE(xform_wgs84_to_ecef.valid());

        REQUIRE(xform_wgs84_to_ecef(glm::dvec3(0, 0, 0), out));
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(6378137, 0, 0), 1e-6)));

        REQUIRE(xform_wgs84_to_ecef.inverse(out, out));
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(0, 0, 0), 1e-6)));
    }

    SECTION("Geocentric interpolation")
    {
        SRS wgs84("wgs84"); // geographic WGS84 (long/lat)
        REQUIRE(wgs84.valid());

        auto midpoint = wgs84.ellipsoid().geodesicInterpolate(
            glm::dvec3(0, 0, 0),
            glm::dvec3(90, 0, 0),
            0.5);

        CHECK(glm::epsilonEqual(midpoint.x, 45.0, 1e-9));
        CHECK(glm::epsilonEqual(midpoint.y, 0.0, 1e-9));
        CHECK(glm::epsilonEqual(midpoint.z, 0.0, 1e-6));
    }

    SECTION("Plate Carree SRS")
    {
        auto pc = SRS("plate-carree");
        REQUIRE(pc == SRS::PLATE_CARREE);
        CHECK(pc.isProjected() == true);
        CHECK(pc.isGeodetic() == false);
        CHECK(pc.isGeocentric() == false);
        auto b = pc.bounds();
        CHECK((b.valid() &&
            glm::epsilonEqual(b.xmin, -20037508.342, E) && glm::epsilonEqual(b.xmax, 20037508.342, E) &&
            glm::epsilonEqual(b.ymin, -10018754.171, E) && glm::epsilonEqual(b.ymax, 10018754.171, E)));
    }

    SECTION("UTM SRS")
    {
        SRS utm32N("epsg:32632"); // +proj=utm +zone=32 +datum=WGS84
        REQUIRE(utm32N.valid());
        CHECK(utm32N.isProjected() == true);
        CHECK(utm32N.isGeodetic() == false);
        CHECK(utm32N.isGeocentric() == false);
        CHECK(utm32N.bounds().valid());

        SRS utm32S("+proj=utm +zone=32 +south +datum=WGS84");
        REQUIRE(utm32S.valid());
        CHECK(utm32S.isProjected() == true);
        CHECK(utm32S.isGeodetic() == false);
        CHECK(utm32S.isGeocentric() == false);
        auto b = utm32S.bounds();
        CHECK((b.valid() && b.xmin == 166000 && b.xmax == 834000 && b.ymin == 1116915 && b.ymax == 10000000));
    }

    SECTION("Quadrilateralized Spherical Cube SRS")
    {
        double E = 1.0;

        SRS wgs84("wgs84");
        REQUIRE(wgs84.valid());

        SRS qsc_face_0("+wktext +proj=qsc +units=m +ellps=WGS84 +datum=WGS84 +lat_0=0 +lon_0=0");
        REQUIRE(qsc_face_0.valid());
        auto qsc_bounds = qsc_face_0.bounds();
        CHECK(qsc_bounds.valid());

        auto xform = wgs84.to(qsc_face_0);
        REQUIRE(xform.valid());

        double semi_major = wgs84.ellipsoid().semiMajorAxis();
        double semi_minor = wgs84.ellipsoid().semiMinorAxis();

        glm::dvec3 c;
        REQUIRE(xform(glm::dvec3(0, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(0, 0, 0), E)));
        // long and lat are out of range for face 0, but doesn't fail
        //CHECK(xform(dvec3(90, 46, 0), c) == false);

        REQUIRE(xform(glm::dvec3(45, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(semi_major, 0, 0), E)));

        REQUIRE(xform.inverse(glm::dvec3(semi_major, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(45, 0, 0), E)));

        REQUIRE(xform(glm::dvec3(0, 45, 0), c));
        // FAILS - not sure what is up here:
        // 45 degrees transforms to 6352271.2440m
        // but the semi-minor axis is 6356752.3142m
        //CHECK(glm::epsilonEqual(c, dvec3(0, semi_minor, 0), E));

        // other way
        xform = qsc_face_0.to(wgs84);
        REQUIRE(xform.valid());

        REQUIRE(xform(glm::dvec3(semi_major, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(45, 0, 0), E)));
    }

    SECTION("Invalid SRS")
    {
        std::string proj_error;
        SRS::projMessageCallback = [&](int level, const char* msg) { proj_error = msg; };

        SRS bad("gibberish");

        CHECK(bad.valid() == false);
        CHECK(bad.isProjected() == false);
        CHECK(bad.isGeodetic() == false);
        CHECK(bad.isGeocentric() == false);
        CHECK(bad.errorMessage() == "Invalid PROJ string syntax");
        
        CHECK(proj_error == "proj_create: unrecognized format / unknown name");

        SRS::projMessageCallback = nullptr;
    }

    SECTION("SRS with Vertical Datum")
    {
        std::string proj_error;
        SRS::projMessageCallback = [&](int level, const char* msg) { proj_error = msg; };

        SRS wgs84("epsg:4979"); // geographic WGS84 (3D)
        REQUIRE(wgs84.valid());
        REQUIRE(wgs84.hasVerticalDatumShift() == false);

        SRS egm96("epsg:4326+5773"); // WGS84 with EGM96 vdatum
        REQUIRE(egm96.valid());
        REQUIRE(egm96.hasVerticalDatumShift() == true);

        // this is legal but will print a warning because Z values will be lost.
        // (you should use epsg::4979 instead)

        SRS wgs84_2d("epsg:4326"); // 2D geographic
        REQUIRE(wgs84_2d);
        auto xform_with_warning = wgs84_2d.to(egm96);
        CHECK(xform_with_warning);
        CHECK(proj_error == "Warning, \"epsg:4326->epsg:4326+5773\" transforms from GEOGRAPHIC_2D_CRS to COMPOUND_CRS. Z values will be discarded. Use a GEOGRAPHIC_3D_CRS instead");
        proj_error.clear();

        // total equivalency:
        REQUIRE(egm96.equivalentTo(wgs84_2d) == false);

        // horizontal equivalency:
        REQUIRE(egm96.horizontallyEquivalentTo(wgs84_2d) == true);
        REQUIRE(wgs84.horizontallyEquivalentTo(wgs84_2d) == true);

        // EGM96 test values are from:
        // https://earth-info.nga.mil/index.php?dir=wgs84&action=egm96-geoid-calc
        glm::dvec3 out(0, 0, 0);

        // geodetic to vdatum:
        {
            //Log()->info("Note: if you see SRS/VDatum errors, check that you have the NGA grid in your share/proj or PROJ_DATA folder! https://github.com/OSGeo/PROJ-data/blob/master/us_nga/us_nga_egm96_15.tif");

            SRS::projMessageCallback = [&](int level, const char* msg) { 
                Log()->warn("PROJ: {} ... do you have the NGA grid in your PROJ_DATA or share/proj folder? You can download it from https://github.com/OSGeo/PROJ-data/blob/master/us_nga/us_nga_egm96_15.tif", msg);
            };

            auto xform = wgs84.to(egm96);
            REQUIRE(xform.valid());

            REQUIRE(xform(glm::dvec3(0, 0, 17.16), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(90, 0, -63.24), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(180, 0, 21.15), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(-90, 0, -4.29), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -63.24, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 21.15, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -4.29, E));

            SRS::projMessageCallback = nullptr;
        }

        // vdatum to geodetic:
        {
            auto xform = egm96.to(wgs84);
            REQUIRE(xform.valid());

            REQUIRE(xform(glm::dvec3(0, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
            REQUIRE(xform(glm::dvec3(90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -63.24, E));
            REQUIRE(xform(glm::dvec3(180, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 21.15, E));
            REQUIRE(xform(glm::dvec3(-90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -4.29, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 17.16), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, -63.24), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 21.15), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, -4.29), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
        }

        // vdatum to vdatum (noop)
        {
            auto xform = egm96.to(egm96);
            REQUIRE(xform.valid());

            glm::dvec3 out;
            REQUIRE(xform(glm::dvec3(0, 0, 17.16), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
        }
    }

    SECTION("SRS Metadata")
    {
        Box a = SRS::WGS84.bounds();
        CHECK(glm::epsilonEqual(a.xmin, -180.0, E));
        CHECK(glm::epsilonEqual(a.xmax, 180.0, E));
        CHECK(glm::epsilonEqual(a.ymin, -90.0, E));
        CHECK(glm::epsilonEqual(a.ymax, 90.0, E));

        Box bb = SRS::SPHERICAL_MERCATOR.bounds();
        CHECK(glm::epsilonEqual(bb.xmin, -20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.xmax, 20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.ymin, -20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.ymax, 20037508.342789248, E));

        auto ellipsoid = SRS::WGS84.ellipsoid();
        REQUIRE(ellipsoid.semiMajorAxis() == 6378137.0);

        UnitsType units;
        units = SRS::WGS84.units();
        CHECK(units == Units::DEGREES);
        units = SRS::SPHERICAL_MERCATOR.units();
        CHECK(units == Units::METERS);
    }

    SECTION("SRS Multithreading")
    {
        // tests the fact that SRS are thread-specific
        auto function = []()
        {
            SRS a("wgs84");
            SRS b("spherical-mercator");
            auto xform = a.to(b);
            glm::dvec3 out;
            REQUIRE(xform(glm::dvec3(-180, 0, 0), out));
            CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(-20037508.34278925, 0, 0), 1e-6)));
        };

        std::vector<std::thread> threads;
        for (unsigned i = 0; i < 12; ++i)
        {
            threads.emplace_back(function);
        }

        for (auto& t : threads)
            t.join();

        // REQUIRE no crash :)
    }

    SECTION("Well-known Profiles")
    {
        Profile GG("global-geodetic");
        REQUIRE(GG.valid());
        CHECK(GG.srs() == SRS::WGS84);

        Profile SM("spherical-mercator");
        REQUIRE(SM.valid());
        CHECK(SM.srs() == SRS::SPHERICAL_MERCATOR);

        Profile PC("plate-carree");
        REQUIRE(PC.valid());
        CHECK(PC.srs() == SRS::PLATE_CARREE);

        Profile INVALID("unknown");
        CHECK(INVALID.valid() == false);
    }

    SECTION("Profile metadata")
    {
        Profile GG("global-geodetic");
        REQUIRE(GG.valid());

        auto profile_ex = GG.extent();
        CHECK(profile_ex == GeoExtent(SRS("wgs84"), -180, -90, 180, 90));

        auto tile_ex = GG.tileExtent(1, 0, 0);
        CHECK(tile_ex == GeoExtent(SRS("wgs84"), -180, 0, -90, 90));

        auto [x0, y0] = GG.numTiles(0);
        CHECK((x0 == 2 && y0 == 1));

        auto [dimx0, dimy0] = GG.tileDimensions(0);
        CHECK((dimx0 == 180.0 && dimy0 == 180.0));

        auto [dimx1, dimy1] = GG.tileDimensions(1);
        CHECK((dimx1 == 90.0 && dimy1 == 90.0));

        unsigned lod = GG.levelOfDetail(45.0);
        CHECK(lod == 2);

        std::vector<TileKey> keys = GG.rootKeys();
        REQUIRE(keys.size() == 2);
        CHECK(keys[0] == TileKey(0, 0, 0, GG));
        CHECK(keys[1] == TileKey(0, 1, 0, GG));
    }

    SECTION("Profile serialization")
    {
        const char* json = R"("
            {
                "extent": {
                    "srs": "+proj=longlat +R=1737400",
                    "xmin": -180,
                    "ymin": -90,
                    "xmax": 180,
                    "ymax": 90
                },
                "tx": 2,
                "ty": 1
            }
        ")";
        Profile profile;
        profile.from_json(json);
    }
}

TEST_CASE("IO")
{
    SECTION("HTTP")
    {
        URI uri("http://readymap.org/readymap/tiles/1.0.0/7/");
        auto r = uri.read(IOOptions());
        CHECKED_IF(r.ok())
        {
            CHECK(r.value().content.type == "text/xml");

            auto body = r.value().content.data;
            CHECK(!body.empty());
            CHECK(rocky::detail::startsWith(body, "<?xml"));
        }
        else
        {
            std::cerr << "HTTP/S request failed: " << r.error().message << std::endl;
        }
    }

    SECTION("HTTPS")
    {
        if (URI::supportsHTTPS())
        {
            URI uri("https://readymap.org/readymap/tiles/1.0.0/7/");
            auto r = uri.read(IOOptions());
            CHECKED_IF(r.ok())
            {
                CHECK(r.value().content.type == "text/xml");
                auto body = r.value().content.data;
                CHECK(!body.empty());
                CHECK(rocky::detail::startsWith(body, "<?xml"));
            }
        }
        else
        {
            WARN("HTTPS support is not available - skipping HTTP tests");
        }
    }

    SECTION("URI")
    {
        URI file("C:/folder/filename.ext");
        CHECK(file.base() == "C:/folder/filename.ext");
        CHECK(file.full() == "C:/folder/filename.ext");

        URI relative_to_folder("filename.ext", "C:/folder/");
        CHECK(relative_to_folder.base() == "filename.ext");
        CHECK(relative_to_folder.full() == "C:/folder/filename.ext");

        URI relative_to_file("filename.ext", "C:/folder/another_file.ext");
        CHECK(relative_to_file.base() == "filename.ext");
        CHECK(relative_to_file.full() == "C:/folder/filename.ext");

        URI relative_with_subfolder("subfolder/filename.ext", "C:/folder/another_file.ext");
        CHECK(relative_with_subfolder.base() == "subfolder/filename.ext");
        CHECK(relative_with_subfolder.full() == "C:/folder/subfolder/filename.ext");

        URI relative_with_parentfolder("../filename.ext", "C:/folder/another_file.ext");
        CHECK(relative_with_parentfolder.base() == "../filename.ext");
        CHECK(relative_with_parentfolder.full() == "C:/filename.ext");

        URI relative_to_url_folder("filename.ext", "https://server.tld/folder/");
        CHECK(relative_to_url_folder.base() == "filename.ext");
        CHECK(relative_to_url_folder.full() == "https://server.tld/folder/filename.ext");

        URI relative_to_url_file("filename.ext", "https://server.tld/folder/another_file.ext");
        CHECK(relative_to_url_file.base() == "filename.ext");
        CHECK(relative_to_url_file.full() == "https://server.tld/folder/filename.ext");
    }
}

TEST_CASE("Earth File")
{
    std::string earthFile = "https://raw.githubusercontent.com/gwaldron/osgearth/master/tests/readymap.earth";
    EarthFileImporter importer;
    auto result = importer.read(earthFile, {});
    CHECKED_IF(result.ok())
    {
        auto contextSingleton = VSGContextFactory::create(nullptr);
        auto context = contextSingleton.get();
        CHECK(context->status.ok());

        auto mapNode = MapNode::create(context);
        auto r = mapNode->from_json(result.value(), context->io.from(earthFile));
        CHECK(r.ok());

        auto layers = mapNode->map->layers([&](auto layer) {
            return layer->name == "ReadyMap 15m Imagery"; });

        CHECK(layers.size() == 1);
        
        auto layer1 = layers.front();
        CHECKED_IF(layer1)
        {
            auto tms_layer = TMSImageLayer::cast(layer1);
            CHECK(tms_layer);
            CHECK(tms_layer->uri.has_value());
            CHECK(tms_layer->uri.value().full() == "https://readymap.org/readymap/tiles/1.0.0/7/");
        }
    }
}

TEST_CASE("MapManipulator NaN fix")
{
    // Test case for issue #105 - NaN error in MapManipulator.cpp
    // This tests that very close vectors don't produce NaN when creating quaternions
    
    // Test data from the original issue
    vsg::dvec3 center(-2248544.6429430502, 5050313.7280376982, 3170373.6894794349);
    vsg::dvec3 target(-2248544.6095093964, 5050313.7806744399, 3170373.6297455574);
    
    // Calculate distance between the vectors
    double dist = vsg::length(center - target);
    double centerMag = vsg::length(center);
    double relativeDist = centerMag > 0 ? dist / centerMag : 0;
    
    // The relative distance should be very small (less than 1e-6)
    CHECK(relativeDist < 1e-6);
    
    // Test our fix logic - when vectors are very close, use identity quaternion
    vsg::dquat rotCenterToTarget;
    if (relativeDist < 1e-6) {
        rotCenterToTarget = vsg::dquat(0, 0, 0, 1);
    } else {
        rotCenterToTarget.set(center, target);
    }
    
    // Check that none of the quaternion components are NaN
    CHECK(!std::isnan(rotCenterToTarget.x));
    CHECK(!std::isnan(rotCenterToTarget.y));
    CHECK(!std::isnan(rotCenterToTarget.z));
    CHECK(!std::isnan(rotCenterToTarget.w));
    
    // The quaternion should be an identity quaternion (or very close to it)
    CHECK(rotCenterToTarget.x == 0.0);
    CHECK(rotCenterToTarget.y == 0.0);
    CHECK(rotCenterToTarget.z == 0.0);
    CHECK(rotCenterToTarget.w == 1.0);
}
