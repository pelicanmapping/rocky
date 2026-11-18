/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "catch.hpp"
#include <rocky/VectorTileLayer.h>
#include <rocky/Map.h>
#include <rocky/vsg/terrain/TerrainState.h>
#include <cstring>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

using namespace ROCKY_NAMESPACE;

TEST_CASE("Terrain vector inputs reject malformed geometry", "[terrain-vectors]")
{
    VectorTile tile;
    REQUIRE(tile.validate().ok()); // a successful empty tile clears inherited content
    VectorTile::Shape shape;
    shape.contours = { { {0, 0}, {1, 0}, {1, 1}, {0, 1} } };
    tile.shapes.push_back(shape);
    REQUIRE(tile.validate().ok());
    tile.shapes[0].contours[0][1].x = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(tile.validate().failed());
    tile.shapes[0] = shape;
    tile.shapes[0].contours.push_back({ {0.1, 0.1}, {0.2, 0.1} });
    REQUIRE(tile.validate().failed()); // malformed hole cannot silently fill in
    tile.shapes = { shape, shape };
    tile.shapes[0].coverageGroup = tile.shapes[1].coverageGroup = 1u;
    for (auto& point : tile.shapes[1].contours[0]) point.x += 1.0;
    REQUIRE(tile.validate().ok());
    tile.shapes[1].color.a = 0.5f;
    REQUIRE(tile.validate().failed()); // one coverage group cannot contain different paints
    tile.shapes[1].color = shape.color;
    tile.shapes[1].type = VectorTile::Shape::Type::Line;
    REQUIRE(tile.validate().failed()); // stroke casing/core have their own paint semantics
}

#ifdef ROCKY_HAS_SLUGHORN
namespace
{
    //! A square and road with distinct translucent colors expose ordering and bin omissions.
    VectorTile orderedArtwork()
    {
        VectorTile result;
        VectorTile::Shape shape;
        shape.contours = { { {0.1, 0.1}, {0.9, 0.1}, {0.9, 0.9}, {0.1, 0.9} },
            { {0.3, 0.3}, {0.7, 0.3}, {0.7, 0.7}, {0.3, 0.7} } };
        shape.color = Color(1, 0, 0, 0.5f);
        result.shapes.push_back(shape);
        shape.type = VectorTile::Shape::Type::Line;
        shape.contours = { { {-0.1, 0.5}, {1.1, 0.5} } };
        shape.width = 0.02f;
        shape.color = Color(0, 0, 1, 0.75f);
        result.shapes.push_back(shape);
        return result;
    }
}

TEST_CASE("Terrain vectors share source ancestors and track mutations", "[terrain-vectors]")
{
    auto map = Map::create();
    auto layer = VectorTileLayer::create();
    layer->maxDataLevel = 2u;
    layer->createTileFunction = [](const TileKey&, const IOOptions&) -> Result<VectorTile> { return orderedArtwork(); };
    REQUIRE(layer->open({}).ok());
    map->add(layer);
    TileKey key(4, 9, 6, Profile("global-geodetic"));
    auto a = terrainVectorRequest(*map, key);
    auto b = terrainVectorRequest(*map, key.createParentKey());
    REQUIRE(a.key == b.key);
    REQUIRE(a.signature == b.signature);
    REQUIRE(a.key.level == 2);
    CHECK(terrainVectorRequest(*map, key, 8.0).signature == a.signature);
    CHECK(terrainVectorRequest(*map, key, 16.0).signature == a.signature);
    layer->opacity = 0.4f;
    REQUIRE(terrainVectorRequest(*map, key).signature != a.signature);
    a = terrainVectorRequest(*map, key);
    layer->dirty();
    REQUIRE(terrainVectorRequest(*map, key).signature != a.signature);
    layer->maxLevel = 3u;
    REQUIRE(terrainVectorRequest(*map, key).sources.empty());
}

TEST_CASE("Terrain vector descriptors preserve painter order and clear empty coverage", "[terrain-vectors]")
{
    auto map = Map::create();
    auto layer = VectorTileLayer::create();
    layer->createTileFunction = [](const TileKey&, const IOOptions&) -> Result<VectorTile> { return orderedArtwork(); };
    REQUIRE(layer->open({}).ok());
    map->add(layer);
    TileKey key(0, 0, 0, Profile("global-geodetic"));
    auto request = terrainVectorRequest(*map, key);
    auto built = buildTerrainVectorTexture(request, {}, 16384u);
    REQUIRE(built.ok());
    auto resource = built.value();
    REQUIRE(resource->layerCount == 2u);
    auto bytes = static_cast<const unsigned char*>(resource->layers->bufferInfoList[0]->data->dataPointer());
    glm::uvec4 header;
    std::memcpy(&header, bytes, 16);
    REQUIRE(header.x == 2u);
    glm::vec4 first, second;
    std::memcpy(&first, bytes + 16, 16);
    std::memcpy(&second, bytes + 16 + sizeof(TerrainVectorLayerGPU), 16);
    REQUIRE(first.r == 1.0f);
    REQUIRE(first.a == 0.5f);
    REQUIRE(second.b == 1.0f);
    REQUIRE(second.a == 0.75f);
    TerrainVectorLayerGPU stroke;
    std::memcpy(&stroke, bytes + 16 + sizeof(stroke), sizeof(stroke));
    REQUIRE(stroke.uvToEmX.w == Approx(0.01f)); // GPU half-width; no buffered outline
    // Every point along the recentered stroke, including caps beyond the carrier, must be indexed.
    for (float u : {-0.1f, 0.0f, 0.5f, 1.0f, 1.1f})
    {
        const glm::vec2 em(u + stroke.uvToEmX.z, 0.5f + stroke.uvToEmY.z);
        const glm::vec2 band = em * glm::vec2(stroke.bandTransform) + glm::vec2(stroke.bandTransform.z, stroke.bandTransform.w);
        CHECK(band.x >= 0.0f);
        CHECK(band.y >= 0.0f);
        CHECK(band.x <= 32.0f);
        CHECK(band.y <= 32.0f);
    }
    const auto* grid = static_cast<const std::uint32_t*>(resource->grid->bufferInfoList[0]->data->dataPointer());
    for (unsigned cell = 0; cell < 256; ++cell)
        for (unsigned j = 1; j < grid[cell * 2 + 1]; ++j)
            REQUIRE(grid[grid[cell * 2] + j - 1] < grid[grid[cell * 2] + j]);
    // Both shapes meet at the center, including the road through the polygon's hole.
    REQUIRE(grid[(8 * 16 + 8) * 2 + 1] == 2u);
    REQUIRE(buildTerrainVectorTexture(request, {}, 1u).failed());
    layer->dirty();
    REQUIRE(buildTerrainVectorTexture(request, {}, 16384u).failed()); // stale request rejected
    request.sources.clear();
    auto empty = buildTerrainVectorTexture(request, {}, 16384u);
    REQUIRE(empty.ok());
    REQUIRE(empty.value()->layerCount == 0u);
    REQUIRE(empty.value()->curves);
    REQUIRE(empty.value()->grid);
}

TEST_CASE("Terrain Mercator strokes preserve metric widths and translucent coverage groups", "[terrain-vectors]")
{
    auto layer = VectorTileLayer::create();
    layer->createTileFunction = [](const TileKey&, const IOOptions&) -> Result<VectorTile> {
        VectorTile tile;
        VectorTile::Shape shape;
        shape.type = VectorTile::Shape::Type::Line;
        shape.coordinates = VectorTile::Shape::Coordinates::Mercator;
        shape.contours = {{{-0.1, 0}, {0.5, 0}, {1.1, 0.1}}};
        shape.width = 0.02f;
        shape.color = Color(1, 0, 0, 0.5f);
        shape.coverageGroup = 7;
        tile.shapes = {shape, shape};
        return tile;
    };
    REQUIRE(layer->open({}).ok());
    for (const auto& profile : {Profile("global-geodetic"), Profile("spherical-mercator")})
    {
        TerrainVectorRequest request;
        request.key = TileKey(4, 8, 5, profile);
        request.sources.push_back({layer, request.key, layer->revision(), 1.0f});
        auto built = buildTerrainVectorTexture(request, {}, 16384u);
        REQUIRE(built.ok());
        REQUIRE(built.value()->layerCount == 2);
        auto bytes = static_cast<const unsigned char*>(built.value()->layers->bufferInfoList[0]->data->dataPointer());
        TerrainVectorLayerGPU a, b;
        std::memcpy(&a, bytes + 16, sizeof(a));
        std::memcpy(&b, bytes + 16 + sizeof(a), sizeof(b));
        CHECK(a.uvToEmX.w == Approx(0.01f));
        CHECK(a.uvToEmY.w == b.uvToEmY.w);
        CHECK(a.color.a == 0.5f);
        CHECK(a.projection.w == (profile.srs() == SRS::WGS84 ? 1.0f : 2.0f));
    }
}

TEST_CASE("Terrain vector inheritance has an independent UV matrix", "[terrain-vectors]")
{
    TerrainTileRenderModel model;
    model.vectors = TerrainVectorTexture::create();
    const auto resource = model.vectors;
    glm::dmat4 northEast(1.0);
    northEast[0][0] = northEast[1][1] = 0.5;
    northEast[3][0] = northEast[3][1] = 0.5;
    model.applyScaleBias(northEast);
    model.applyScaleBias(northEast);
    REQUIRE(model.vectors == resource);
    REQUIRE(model.vectorMatrix[0][0] == 0.25);
    REQUIRE(model.vectorMatrix[3][0] == 0.75);
    REQUIRE(model.vectorMatrix[3][1] == 0.75);
    REQUIRE(model.color.matrix[0][0] == 1.0); // no color image to inherit
}

TEST_CASE("Terrain stroke paint updates reuse bounded geometry", "[terrain-vectors]")
{
    float width = 0.02f;
    Color color(1, 0, 0, 0.5f);
    auto layer = VectorTileLayer::create();
    layer->createTileFunction = [&](const TileKey&, const IOOptions&) -> Result<VectorTile> {
        VectorTile tile;
        VectorTile::Shape shape;
        shape.type = VectorTile::Shape::Type::Line;
        shape.contours = {{{0.1, 0.1}, {0.5, 0.8}, {0.9, 0.1}}};
        shape.width = width;
        shape.color = color;
        tile.shapes.push_back(shape);
        return tile;
    };
    REQUIRE(layer->open({}).ok());
    auto map = Map::create();
    map->add(layer);
    TileKey key(2, 1, 1, Profile("global-geodetic"));
    auto first = buildTerrainVectorTexture(terrainVectorRequest(*map, key), {}, 16384u);
    REQUIRE(first.ok());
    width = 0.025f; // both widths fit the 0.03125 acceleration envelope
    color = Color(0, 0, 1, 0.75f);
    layer->dirty();
    auto second = buildTerrainVectorTexture(terrainVectorRequest(*map, key), {}, 16384u);
    REQUIRE(second.ok());
    CHECK(second.value()->reusedGeometry);
    CHECK(second.value()->curves == first.value()->curves);
    CHECK(second.value()->bands == first.value()->bands);
    CHECK(second.value()->layers != first.value()->layers);
    TerrainVectorLayerGPU record;
    auto bytes = static_cast<const unsigned char*>(second.value()->layers->bufferInfoList[0]->data->dataPointer());
    std::memcpy(&record, bytes + 16, sizeof(record));
    CHECK(record.uvToEmX.w == Approx(0.0125f));
    CHECK(record.color.b == 1.0f);
    CHECK(record.color.a == 0.75f);
    width = 0.04f; // a wider stroke must rebuild the acceleration envelope
    layer->dirty();
    auto wider = buildTerrainVectorTexture(terrainVectorRequest(*map, key), {}, 16384u);
    REQUIRE(wider.ok());
    CHECK(wider.value()->curves != first.value()->curves);
    CHECK(wider.value()->bands != first.value()->bands);
}
#endif

TEST_CASE("Terrain view style scale follows cameras without a manipulator", "[terrain-vectors]")
{
    const auto perspective = glm::perspective(glm::radians(30.0), 1.0, 1.0, 1e8);
    const auto view = glm::lookAt(glm::dvec3(0, 0, 1000), glm::dvec3(0), glm::dvec3(0, 1, 0));
    const double expected = std::log2(40075016.68557849 / (512.0 * 2000.0 * std::tan(glm::radians(15.0)) / 800.0));
    const auto zoom = terrainViewStyleZoom(SRS::SPHERICAL_MERCATOR, view, perspective, 800.0);
    REQUIRE(zoom.has_value());
    CHECK(*zoom == Approx(expected));
    CHECK(*terrainViewStyleZoom(SRS::SPHERICAL_MERCATOR, view, perspective, 1600.0) == Approx(expected + 1.0));
    const auto ortho = glm::ortho(-400.0, 400.0, -400.0, 400.0, 1.0, 1e8);
    CHECK(*terrainViewStyleZoom(SRS::SPHERICAL_MERCATOR, view, ortho, 800.0) ==
        Approx(std::log2(40075016.68557849 / 512.0)));
    const double radius = SRS::ECEF.ellipsoid().semiMajorAxis();
    const auto globe = glm::lookAt(glm::dvec3(radius + 1000, 0, 0), glm::dvec3(radius, 0, 0), glm::dvec3(0, 0, 1));
    REQUIRE(terrainViewStyleZoom(SRS::ECEF, globe, perspective, 800.0).has_value());
    CHECK(*terrainViewStyleZoom(SRS::ECEF, globe, perspective, 800.0) == Approx(expected));
    CHECK_FALSE(terrainViewStyleZoom(SRS::ECEF, globe, perspective, 0.0).has_value());
    const auto away = glm::lookAt(glm::dvec3(radius + 1000, 0, 0), glm::dvec3(radius + 2000, 0, 0), glm::dvec3(0, 0, 1));
    CHECK_FALSE(terrainViewStyleZoom(SRS::ECEF, away, perspective, 800.0).has_value());
    CHECK_FALSE(terrainViewStyleZoom(SRS::WGS84, view, perspective, 800.0).has_value());
    const auto flat = terrainViewStyleZoom(SRS::PLATE_CARREE, view, perspective, 800.0);
    REQUIRE(flat.has_value());
    CHECK(std::abs(*flat-expected) < 0.02);
}
