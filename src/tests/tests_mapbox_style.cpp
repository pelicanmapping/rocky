/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "catch.hpp"
#include <rocky/MapboxStyle.h>
#include <rocky/MapboxVectorTileLayer.h>
#include <rocky/FileContentCache.h>
#include <rocky/Context.h>
#include <rocky/Map.h>
#include <rocky/GeoPoint.h>
#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstring>

using namespace ROCKY_NAMESPACE;

TEST_CASE("MapLibre expressions retain filter types and continuous zoom", "[mapbox-style]")
{
    MapboxStyle style;
    REQUIRE(style
                .load(R"json({"version":8,"sources":{"s":{"type":"vector","tiles":["{z}/{x}/{y}.pbf"]}},"layers":[
      {"id":"road","type":"line","source":"s","source-layer":"roads","minzoom":2,"maxzoom":10,
       "filter":["all",["in",["get","class"],["literal",["primary","motorway"]]],["==",["get","level"],1]],
       "paint":{"line-color":["match",["get","class"],"primary","hsl(120,100%,50%)","#ff0000"],
       "line-width":["interpolate",["linear"],["zoom"],2,2,6,10],"line-opacity":0.5}},
      {"id":"legacy","type":"fill","source":"s","source-layer":"land",
       "filter":["all",["==","class","park"],["==","park",["get","class"]],
        ["in","park",["literal",["park","garden"]]]],
       "paint":{"fill-color":"#ffffff"}},
      {"id":"unsupported","type":"line","source":"s","source-layer":"roads","paint":{"line-dasharray":[1,2]}}
    ]})json")
                .ok());
    REQUIRE(style.layers().size() == 2);
    REQUIRE(style.diagnostics().size() == 1);
    Feature f;
    f.geometry.type = Geometry::Type::LineString;
    f.fields["class"].emplace<std::string>("primary");
    f.fields["level"].emplace<std::int64_t>(1);
    auto p = style.evaluate(0, f, 3.5);
    REQUIRE(p.ok());
    REQUIRE(p.value());
    CHECK(p.value()->widthPixels == Approx(5));
    CHECK(p.value()->color.g == Approx(1));
    CHECK(p.value()->color.r == Approx(0));
    CHECK(p.value()->color.a == Approx(0.5));
    REQUIRE(!style.evaluate(0, f, 10).value());
    f.fields["level"].emplace<std::string>("1");
    REQUIRE(!style.evaluate(0, f, 4).value());
    f.fields["class"].emplace<std::string>("park");
    REQUIRE(style.evaluate(1, f, 4).value());
    REQUIRE(style.load("{}").failed());
    REQUIRE(style.layers().size() == 2); // transactional replacement
}

TEST_CASE("MapLibre lazy branches color ramps and strict diagnostics", "[mapbox-style]")
{
    MapboxStyle style;
    REQUIRE(style
                .load(R"({"version":8,"sources":{},"layers":[
      {"id":"bg","type":"background","paint":{"background-color":["interpolate",["linear"],["zoom"],0,"#000000",10,"#ffffff"],
       "background-opacity":["case",["has","opacity"],["get","opacity"],1]}},
      {"id":"bad","type":"background","paint":{"background-color":["mystery",1]}}
    ]})")
                .ok());
    REQUIRE(style.layers().size() == 1);
    REQUIRE(style.diagnostics().size() == 1);
    auto p = style.evaluate(0, {}, 5);
    REQUIRE(p.ok());
    REQUIRE(p.value());
    CHECK(p.value()->color.r == Approx(0.214041f));
    CHECK(p.value()->color.a == 1);
    Feature f;
    f.fields["opacity"].emplace<double>(0);
    REQUIRE(!style.evaluate(0, f, 5).value());
}

TEST_CASE("Vector map configuration round trips through the layer factory", "[mapbox-style][serialization]")
{
    auto context = ContextFactory::create();
    const auto io = context.get()->io.from("https://example.org/maps/world.map.json");
    auto map = Map::create();
    REQUIRE(map->from_json(R"({"name":"Vector map","layers":[{
        "type":"MapboxVectorTile","name":"Roads","open":false,"opacity":0.6,
        "attribution":{"text":"Fixture","href":"https://example.org"},
        "minLevel":2,"maxLevel":19,"maxDataLevel":14,"profile":"global-geodetic",
        "style":{"href":"styles/roads.json","headers":[["X-Test","fixture"]]},
        "decodedCacheBytes":1048576,"maximumSourceZoom":13,
        "forceBufferedStrokes":true,"automaticStyleZoom":false,"styleZoom":10.36
    }]})", io).ok());
    for (unsigned pass = 0; pass < 2; ++pass)
    {
        REQUIRE(map->layers().size() == 1);
        auto layer = MapboxVectorTileLayer::cast(map->layers().front());
        REQUIRE(layer);
        CHECK(layer->name == "Roads");
        CHECK(!layer->openAutomatically);
        CHECK(layer->opacity.value() == Approx(0.6));
        CHECK(layer->minLevel.value() == 2);
        CHECK(layer->maxLevel.value() == 19);
        CHECK(layer->maxDataLevel.value() == 14);
        CHECK(layer->attribution->text == "Fixture");
        CHECK(layer->styleURI.base() == "styles/roads.json");
        CHECK(layer->styleURI.full() == "https://example.org/maps/styles/roads.json");
        REQUIRE(layer->styleURI.context().headers.size() == 1);
        CHECK(layer->styleURI.context().headers.front().first == "X-Test");
        CHECK(layer->styleURI.context().headers.front().second == "fixture");
        CHECK(!json::parse(layer->to_json()).contains("cacheDirectory"));
        CHECK(layer->decodedCacheBytes == 1048576);
        CHECK(layer->maximumSourceZoom == 13);
        CHECK(layer->forceBufferedStrokes);
        CHECK(!layer->automaticStyleZoom);
        CHECK(layer->styleZoom() == 10.25);
        CHECK(layer->viewStyleZoom(18).value() == 10.25);
        CHECK(!layer->isOpen());
        REQUIRE(map->openAllLayers(io).ok()); // disabled layers do not perform source I/O
        CHECK(!layer->isOpen());
        const auto saved = json::parse(map->to_json());
        CHECK(saved["layers"][0]["profile"] == "global-geodetic");
        CHECK(saved["layers"][0]["type"] == "MapboxVectorTile");
        CHECK(!saved["layers"][0].contains("stats"));
        auto restored = Map::create();
        REQUIRE(restored->from_json(saved.dump(), io).ok());
        map = restored;
    }
    auto relocated = MapboxVectorTileLayer::create(map->layers().front()->to_json(),
        context.get()->io.from("https://example.net/moved/world.map.json"));
    CHECK(relocated->styleURI.full() == "https://example.net/moved/styles/roads.json");
}

TEST_CASE("Vector JSON retains defaults and rejects invalid provider options", "[mapbox-style][serialization]")
{
    auto context = ContextFactory::create();
    auto defaults = MapboxVectorTileLayer::create(R"({"style":"style.json"})", context.get()->io);
    auto restored = MapboxVectorTileLayer::create(defaults->to_json(), context.get()->io);
    CHECK(restored->automaticStyleZoom);
    CHECK(!restored->forceBufferedStrokes);
    CHECK(!json::parse(restored->to_json()).contains("cacheDirectory"));
    CHECK(restored->decodedCacheBytes == 128u * 1024u * 1024u);
    CHECK(restored->maximumSourceZoom == 14);
    CHECK(restored->styleZoom() == 12.0);
    CHECK(restored->viewStyleZoom(15.1).value() == 15.0);
    for (const auto* input : { "{}", "[]", "{", R"({"style":42})", R"({"style":""})",
        R"({"style":{"href":true}})", R"({"style":"unused","cacheDirectory":true})",
        R"({"style":"unused","decodedCacheBytes":-1})", R"({"style":"unused","decodedCacheBytes":1.5})",
        R"({"style":"unused","maximumSourceZoom":23})", R"({"style":"unused","maximumSourceZoom":-1})",
        R"({"style":"unused","automaticStyleZoom":"false"})", R"({"style":"unused","forceBufferedStrokes":1})",
        R"({"style":"unused","styleZoom":null})" })
    {
        INFO(input);
        auto invalid = MapboxVectorTileLayer::create(input, context.get()->io);
        auto opened = invalid->open(context.get()->io);
        REQUIRE(opened.failed());
        CHECK(!invalid->isOpen());
    }
}

#if defined(ROCKY_HAS_GDAL) && defined(ROCKY_HAS_SLUGHORN)
namespace
{
    //! Minimal protobuf writer for a deterministic, genuine MVT fixture; no network or GDAL writer required.
    void varint(std::string& out, std::uint64_t v)
    {
        while (v > 127)
        {
            out.push_back(static_cast<char>((v & 127) | 128));
            v >>= 7;
        }
        out.push_back(static_cast<char>(v));
    }
    //! Writes a protobuf unsigned scalar field.
    void scalar(std::string& out, unsigned field, std::uint64_t v)
    {
        varint(out, field << 3);
        varint(out, v);
    }
    //! Writes a length-delimited protobuf field.
    void bytes(std::string& out, unsigned field, const std::string& value)
    {
        varint(out, (field << 3) | 2);
        varint(out, value.size());
        out += value;
    }
    //! Encodes buffered clockwise exterior and opposite-wound hole in the MVT y-down coordinate system.
    std::string fixture(int centerX = 2048, int centerY = 2048, int halfSize = 512)
    {
        std::string geom;
        int x = 0, y = 0;
        auto point = [&](int nx, int ny)
        {
            nx = centerX + (nx-2048)*halfSize/512;
            ny = centerY + (ny-2048)*halfSize/512;
            auto zig = [](int v) { return static_cast<unsigned>(v < 0 ? -2 * v - 1 : 2 * v); };
            varint(geom, zig(nx - x));
            varint(geom, zig(ny - y));
            x = nx;
            y = ny;
        };
        varint(geom, 9);
        point(1536, 1536);
        varint(geom, 26);
        point(2560, 1536);
        point(2560, 2560);
        point(1536, 2560);
        varint(geom, 15);
        varint(geom, 9);
        point(1792, 1792);
        varint(geom, 26);
        point(1792, 2304);
        point(2304, 2304);
        point(2304, 1792);
        varint(geom, 15);
        std::string feature;
        scalar(feature, 1, 42);
        scalar(feature, 3, 3);
        bytes(feature, 4, geom);
        std::string layer;
        bytes(layer, 1, "land");
        bytes(layer, 2, feature);
        scalar(layer, 5, 4096);
        scalar(layer, 15, 2);
        std::string tile;
        bytes(tile, 3, layer);
        geom.clear();
        x = y = 0;
        varint(geom, 9);
        point(-80, 2048);
        varint(geom, 10);
        point(4176, 2048);
        feature.clear();
        scalar(feature, 1, 7);
        scalar(feature, 3, 2);
        bytes(feature, 4, geom);
        std::string tags;
        for (unsigned v : {0u, 0u, 1u, 1u, 2u, 2u})
            varint(tags, v);
        bytes(feature, 2, tags);
        layer.clear();
        bytes(layer, 1, "roads");
        bytes(layer, 2, feature);
        scalar(layer, 5, 4096);
        scalar(layer, 15, 2);
        bytes(layer, 3, "class");
        bytes(layer, 3, "rank");
        bytes(layer, 3, "visible");
        std::string value;
        bytes(value, 1, "primary");
        bytes(layer, 4, value);
        value.clear();
        scalar(value, 4, 1);
        bytes(layer, 4, value);
        value.clear();
        scalar(value, 7, 1);
        bytes(layer, 4, value);
        bytes(tile, 3, layer);
        return tile;
    }
    //! Test-scoped directory; removes only the files authored by this fixture.
    struct Files
    {
        std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("rocky-map-style-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Files()
        {
            std::filesystem::create_directories(directory);
        }
        ~Files()
        {
            std::error_code ec;
            std::filesystem::remove_all(directory, ec);
        }
    };
} // namespace
TEST_CASE("Styled MVT preserves holes batches strokes and reuses source detail", "[mapbox-style][mvt]")
{
    auto context = ContextFactory::create();
    Files files;
    auto tilePath = (files.directory / "0-0-0.pbf").generic_string();
    auto stylePath = (files.directory / "style.json").generic_string();
    {
        std::ofstream out(tilePath, std::ios::binary);
        out << fixture();
    }
    {
        std::ofstream out(stylePath);
        out << R"({"version":8,"sources":{"s":{"type":"vector","url":"tiles.json"}},"layers":[
      {"id":"land","type":"fill","source":"s","source-layer":"land","paint":{"fill-color":"#00ff00"}},
      {"id":"roads","type":"line","source":"s","source-layer":"roads",
       "filter":["all",["==",["get","class"],"primary"],["==",["get","rank"],1],["==",["get","visible"],true]],
       "paint":{"line-color":"#ff0000","line-width":4}}
      ]})";
    }
    {
        std::ofstream out(files.directory / "tiles.json");
        out << R"({"minzoom":0,"maxzoom":0,"tiles":["{z}-{x}-{y}.pbf"]})";
    }
    auto map = Map::create();
    REQUIRE(map->from_json(R"({"layers":[{"type":"MapboxVectorTile","style":"style.json","styleZoom":4}]})",
        context.get()->io.from((files.directory / "map.json").generic_string())).ok());
    REQUIRE(map->layers().size() == 1);
    auto layer = MapboxVectorTileLayer::cast(map->layers().front());
    REQUIRE(layer);
    REQUIRE(map->openAllLayers(context.get()->io).ok());
    auto tile = layer->createTile(TileKey(0, 0, 0, Profile("spherical-mercator")), context.get()->io);
    INFO((tile.failed() ? tile.error().message : ""));
    REQUIRE(tile.ok());
    REQUIRE(tile.value().validate().ok());
    REQUIRE(tile.value().shapes.size() == 2);
    REQUIRE(tile.value().shapes[0].polygons.size() == 1);
    CHECK(tile.value().shapes[0].polygons[0].size() == 2); // hole survived decoding, clipping and union
    CHECK(tile.value().shapes[1].color.r == 1);
    CHECK(tile.value().shapes[1].type == VectorTile::Shape::Type::Line);
    CHECK(tile.value().shapes[1].coordinates == VectorTile::Shape::Coordinates::Mercator);
    CHECK(tile.value().shapes[1].width == Approx(4.0 / (512.0 * 16.0)));
    CHECK(layer->stats().gpuStrokePieces == 1);
    CHECK(layer->stats().bufferedStrokeBatches == 0);
    auto stats = layer->stats();
    REQUIRE(stats.reads == 1);
    layer->setStyleZoom(4.11);
    CHECK(layer->styleZoom() == 4); // stable quarter-zoom bucket
    layer->setStyleZoom(5);
    auto fine = layer->createTile(TileKey(1, 1, 0, Profile("global-geodetic")), context.get()->io);
    INFO((fine.failed() ? fine.error().message : ""));
    REQUIRE(fine.ok());
    REQUIRE(fine.value().validate().ok());
    CHECK(layer->stats().reads == 1);
    CHECK(layer->stats().hits > 0);
    // An engine request carries its own view style, without changing the layer or another view.
    const auto revision = layer->revision();
    const auto viewZoom = layer->viewStyleZoom(4.11);
    REQUIRE(viewZoom.has_value());
    CHECK(*viewZoom == 4.0);
    auto snapshot = layer->createTile(TileKey(0, 0, 0, Profile("spherical-mercator")), context.get()->io, viewZoom);
    REQUIRE(snapshot.ok());
    CHECK(snapshot.value().shapes[1].width == Approx(4.0 / (512.0 * 16.0)));
    CHECK(layer->styleZoom() == 5.0);
    CHECK(layer->revision() == revision);
    auto buffered = MapboxVectorTileLayer::create();
    buffered->styleURI = URI(stylePath);
    buffered->forceBufferedStrokes = true;
    buffered->setStyleZoom(4);
    REQUIRE(buffered->open(context.get()->io).ok());
    auto fallback = buffered->createTile(TileKey(0, 0, 0, Profile("spherical-mercator")), context.get()->io);
    REQUIRE(fallback.ok());
    REQUIRE(fallback.value().shapes.size() == 2);
    CHECK(fallback.value().shapes[1].type == VectorTile::Shape::Type::Fill);
    CHECK(buffered->stats().bufferedStrokeBatches == 1);
    // A transparent center is a true gap, so it must retain ring coverage rather than paint a solid casing.
    std::ifstream original(stylePath);
    std::string gapStyle((std::istreambuf_iterator<char>(original)), {});
    original.close();
    const auto widthProperty = gapStyle.find("\"line-width\":4");
    REQUIRE(widthProperty != std::string::npos);
    gapStyle.replace(widthProperty, std::strlen("\"line-width\":4"), "\"line-width\":4,\"line-gap-width\":2");
    { std::ofstream out(stylePath); out << gapStyle; }
    auto gapLayer = MapboxVectorTileLayer::create();
    gapLayer->styleURI = URI(stylePath);
    gapLayer->setStyleZoom(4);
    REQUIRE(gapLayer->open(context.get()->io).ok());
    auto gap = gapLayer->createTile(TileKey(0, 0, 0, Profile("spherical-mercator")), context.get()->io);
    REQUIRE(gap.ok());
    CHECK(gapLayer->stats().gpuStrokePieces == 0);
    CHECK(gapLayer->stats().bufferedStrokeBatches == 1);
}

TEST_CASE("MVT sources share persistent content caching across layers", "[mapbox-style][content-cache]")
{
    auto context = ContextFactory::create();
    Files files;
    const auto source = files.directory / "0-0-0.pbf";
    const auto style = files.directory / "style.json";
    const auto metadata = files.directory / "tiles.json";
    { std::ofstream out(source, std::ios::binary); out << fixture(); }
    { std::ofstream out(metadata); out << R"({"minzoom":0,"maxzoom":0,"tiles":["{z}-{x}-{y}.pbf"]})"; }
    {
        std::ofstream out(style);
        out << R"({"version":8,"sources":{"s":{"type":"vector","url":"tiles.json"}},"layers":[
            {"id":"land","type":"fill","source":"s","source-layer":"land","paint":{"fill-color":"#00ff00"}}]})";
    }
    const auto cachePath = (files.directory / "content").string();
    auto& io = context.get()->io;
    io.services().contentCache = std::make_shared<FileContentCache>(cachePath, 0);
    for (unsigned pass = 0; pass < 2; ++pass)
    {
        auto layer = MapboxVectorTileLayer::create();
        layer->styleURI = URI(style.generic_string());
        layer->decodedCacheBytes = 0;
        REQUIRE(layer->open(io).ok());
        const auto tile = layer->createTile(TileKey(0, 0, 0, Profile("spherical-mercator")), io);
        REQUIRE(tile.ok());
        REQUIRE(!tile->shapes.empty());
        CHECK(layer->stats().reads == (pass == 0 ? 1 : 0));
        CHECK(layer->stats().hits > 0); // wrapped world copies also reuse this source URI
        CHECK(layer->stats().cachedTiles == 0);
        if (pass == 0)
        {
            std::filesystem::remove(source);
            std::filesystem::remove(style);
            std::filesystem::remove(metadata);
            io.services().contentCache = std::make_shared<FileContentCache>(cachePath, 0);
        }
    }
}

TEST_CASE("Styled vectors reproject into plate carree and every cube face", "[mapbox-style][profiles]")
{
    auto context = ContextFactory::create();
    Files files;
    { std::ofstream out(files.directory / "0-0-0.pbf", std::ios::binary); out << fixture(); }
    {
        std::ofstream out(files.directory / "style.json");
        out << R"({"version":8,"sources":{"s":{"type":"vector","minzoom":0,"maxzoom":0,"tiles":["{z}-{x}-{y}.pbf"]}},
            "layers":[{"id":"land","type":"fill","source":"s","source-layer":"land","paint":{"fill-color":"#00ff00"}},
            {"id":"road","type":"line","source":"s","source-layer":"roads","paint":{"line-color":"#ff0000",
            "line-width":4}}]})";
    }
    auto layer = MapboxVectorTileLayer::create();
    layer->styleURI = URI((files.directory / "style.json").generic_string());
    REQUIRE(layer->open(context.get()->io).ok());
    std::vector<Profile> profiles = Profile("global-qsc").subprofiles();
    profiles.push_back(Profile("plate-carree"));
    unsigned filledPieces = 0;
    for (const auto& profile : profiles)
    for (const auto& key : profile.rootKeys())
    {
        INFO(profile.srs().definition());
        INFO(key.str());
        auto tile = layer->createTile(key, context.get()->io, 4.0);
        INFO((tile.failed() ? tile.error().message : ""));
        REQUIRE(tile.ok());
        REQUIRE(tile.value().validate().ok());
        for (const auto& shape : tile.value().shapes)
        {
            CHECK(shape.coordinates == VectorTile::Shape::Coordinates::TileUV);
            if (shape.color.g > 0.5f && !shape.polygons.empty()) ++filledPieces;
        }
    }
    CHECK(filledPieces > 0);
    CHECK(layer->stats().bufferedStrokeBatches > 0);
    // A small, nearly conformal equatorial tile still uses GPU centerlines.
    const auto key = TileKey::createTileKeyContainingPoint(GeoPoint(SRS::WGS84, 1.0, 0.0), 10, Profile("plate-carree"));
    const auto before = layer->stats().gpuStrokePieces;
    auto fine = layer->createTile(key, context.get()->io, 10.0);
    INFO((fine.failed() ? fine.error().message : ""));
    REQUIRE(fine.ok());
    REQUIRE(fine.value().validate().ok());
    CHECK(layer->stats().gpuStrokePieces > before);
}

TEST_CASE("Projected vector fills preserve holes near poles and the date line", "[mapbox-style][profiles]")
{
    auto context = ContextFactory::create();
    const auto faces = Profile("global-qsc").subprofiles();
    // Sample the resulting green polygon coverage independently of its ring winding.
    const auto contains = [](const VectorTile& tile, glm::dvec2 p)
    {
        const auto ringContains = [&](const std::vector<glm::dvec2>& ring)
        {
            bool inside = false;
            for (std::size_t i = 0, j = ring.size()-1; i < ring.size(); j = i++)
                if ((ring[i].y > p.y) != (ring[j].y > p.y) &&
                    p.x < (ring[j].x-ring[i].x)*(p.y-ring[i].y)/(ring[j].y-ring[i].y)+ring[i].x)
                    inside = !inside;
            return inside;
        };
        for (const auto& shape : tile.shapes)
        for (const auto& polygon : shape.polygons)
        {
            bool covered = ringContains(polygon.front());
            for (std::size_t i = 1; i < polygon.size(); ++i) covered &= !ringContains(polygon[i]);
            if (covered) return true;
        }
        return false;
    };
    for (const auto& sample : { std::array<int,4>{0,2200,64,32}, {1,2200,4032,32},
        {4,4080,2100,8}, {4,16,2100,8}, {2,2200,2100,64} })
    {
        INFO(sample[0]);
        Files files;
        { std::ofstream out(files.directory / "0.pbf", std::ios::binary); out << fixture(sample[1], sample[2], sample[3]); }
        {
            std::ofstream out(files.directory / "style.json");
            out << R"({"version":8,"sources":{"s":{"type":"vector","minzoom":0,"maxzoom":0,"tiles":["0.pbf"]}},
                "layers":[{"id":"land","type":"fill","source":"s","source-layer":"land",
                "paint":{"fill-color":"#00ff00"}}]})";
        }
        auto layer = MapboxVectorTileLayer::create();
        layer->styleURI = URI((files.directory / "style.json").generic_string());
        REQUIRE(layer->open(context.get()->io).ok());
        std::vector<Profile> targets{faces[sample[0]], Profile("plate-carree")};
        for (const auto& target : targets)
        for (const bool hole : {false, true})
        {
            const double x = sample[1]+(hole ? 0.0 : sample[3]*0.75);
            const double y = sample[2];
            const GeoPoint point(SRS::SPHERICAL_MERCATOR,
                (x/4096.0-0.5)*40075016.68557849, (0.5-y/4096.0)*40075016.68557849);
            const auto key = TileKey::createTileKeyContainingPoint(point, 3, target);
            REQUIRE(key.valid());
            auto tile = layer->createTile(key, context.get()->io, 8.0);
            INFO((tile.failed() ? tile.error().message : ""));
            REQUIRE(tile.ok());
            REQUIRE(tile.value().validate().ok());
            auto projected = point.transform(target.srs());
            REQUIRE(projected.valid());
            const auto extent = key.extent();
            CHECK(contains(tile.value(), {(projected.x-extent.xmin())/extent.width(),
                (projected.y-extent.ymin())/extent.height()}) == !hole);
        }
    }
}
#endif
