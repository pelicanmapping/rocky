/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "catch.hpp"
#include <rocky/StyledFeatureLayer.h>
#include <rocky/GDALFeatureSource.h>
#include <rocky/TiledFeatureSource.h>
#include <rocky/Context.h>
#include <rocky/Map.h>
#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <future>
#ifdef ROCKY_HAS_GDAL
#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Creates a standard single-collection style; source loading is supplied by the Rocky binding.
    json roadStyle()
    {
        return json::parse(R"({"version":8,"sources":{"roads":{"type":"geojson",
            "data":{"type":"FeatureCollection","features":[]}}},"layers":[
            {"id":"road","type":"line","source":"roads","filter":["==",["get","Kind"],"primary"],
            "layout":{"line-cap":"round","line-join":"round"},
            "paint":{"line-color":"#00ff00","line-width":3}}]})");
    }
}

TEST_CASE("Styled features round trip inline styles and named source bindings", "[styled-features][serialization]")
{
    auto context = ContextFactory::create();
    auto io = context.get()->io.from("https://example.org/maps/world.json");
    json config = {{"type","StyledFeature"}, {"open",false}, {"style",roadStyle()},
        {"sourceBindings",{{"roads",{{"type","GDAL"},{"uri","roads.shp"}}}}}};
    auto map = Map::create();
    REQUIRE(map->from_json(json{{"layers",json::array({config})}}.dump(), io).ok());
    auto layer = StyledFeatureLayer::cast(map->layers().front());
    REQUIRE(layer);
    CHECK(json::parse(layer->styleJSON) == roadStyle());
    CHECK(layer->styleURI.empty());
    auto saved = json::parse(layer->to_json());
    CHECK(saved["type"] == "StyledFeature");
    CHECK(saved["style"] == config["style"]);
    CHECK(saved["sourceBindings"] == config["sourceBindings"]);
    auto restored = StyledFeatureLayer::create(saved.dump(), io);
    CHECK(json::parse(restored->to_json())["style"] == roadStyle());
    MapboxStyle style;
    REQUIRE(style.load(roadStyle().dump()).ok());
    REQUIRE(style.layers().size() == 1);
    CHECK(style.layers().front().sourceLayer.empty());
    auto invalid = config;
    invalid["style"]["href"] = "ambiguous.json";
    CHECK(StyledFeatureLayer::create(invalid.dump(), io)->open(io).failed());
}

#ifdef ROCKY_HAS_GDAL
namespace
{
    //! Owns temporary multi-file GDAL fixtures for the duration of a test.
    struct FeatureFiles
    {
        std::filesystem::path directory;
        //! Creates a unique directory without touching project data.
        FeatureFiles()
        {
            directory = std::filesystem::temp_directory_path() / ("rocky-styled-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(directory);
        }
        //! Removes only this fixture's temporary directory after datasets close.
        ~FeatureFiles() { std::error_code error; std::filesystem::remove_all(directory, error); }

        //! Writes one typed line feature in the requested GDAL vector format and CRS.
        std::string create(const std::string& filename, const char* driver, bool indexed = true,
            int epsg = 3857, const char* wkt = "LINESTRING (-1000000 -1000000, 1000000 1000000)")
        {
            GDALAllRegister();
            const auto path = (directory / filename).generic_string();
            auto ds = GDALCreate(GDALGetDriverByName(driver), path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
            REQUIRE(ds);
            auto sr = OSRNewSpatialReference(nullptr);
            REQUIRE(OSRImportFromEPSG(sr, epsg) == OGRERR_NONE);
            OSRSetAxisMappingStrategy(sr, OAMS_TRADITIONAL_GIS_ORDER);
            char disabled[] = "SPATIAL_INDEX=NO";
            char* options[] = { disabled, nullptr };
            const auto geometryType = std::string(wkt).find("POLYGON") == 0 ? wkbPolygon : wkbLineString;
            auto layer = GDALDatasetCreateLayer(ds, "roads", sr, geometryType,
                !indexed && std::string(driver) == "FlatGeobuf" ? options : nullptr);
            REQUIRE(layer);
            OSRDestroySpatialReference(sr);
            auto field = OGR_Fld_Create("Kind", OFTString);
            REQUIRE(OGR_L_CreateField(layer, field, 1) == OGRERR_NONE);
            OGR_Fld_Destroy(field);
            auto feature = OGR_F_Create(OGR_L_GetLayerDefn(layer));
            OGR_F_SetFieldString(feature, 0, "primary");
            OGRGeometryH geometry = nullptr;
            auto cursor = const_cast<char*>(wkt);
            REQUIRE(OGR_G_CreateFromWkt(&cursor, nullptr, &geometry) == OGRERR_NONE);
            REQUIRE(OGR_F_SetGeometryDirectly(feature, geometry) == OGRERR_NONE);
            REQUIRE(OGR_L_CreateFeature(layer, feature) == OGRERR_NONE);
            OGR_F_Destroy(feature);
            GDALClose(ds);
            return path;
        }
    };
}

TEST_CASE("GDAL spatial queries preserve fields and enforce bounded complete results", "[styled-features][feature-query]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    for (const auto& format : {std::pair<const char*,const char*>("indexed.fgb","FlatGeobuf"),
        {"unindexed.fgb","FlatGeobuf"}, {"roads.shp","ESRI Shapefile"}, {"roads.gpkg","GPKG"}})
    {
        INFO(format.first);
        const auto path = files.create(format.first, format.second, std::string(format.first) != "unindexed.fgb");
        auto source = GDALFeatureSource::create();
        source->uri = URI(path);
        source->spatialQueriesOnly = true;
        REQUIRE(source->open().ok());
        FeatureSource::Query query;
        query.extent = GeoExtent(SRS::SPHERICAL_MERCATOR,-2000000,-2000000,2000000,2000000);
        auto result = source->query(query, context.get()->io);
        REQUIRE(result.ok());
        REQUIRE(result.value()->records.size() == 1);
        const auto& feature = result.value()->records.front().feature;
        CHECK(feature.hasField("Kind"));
        CHECK(!feature.hasField("kind"));
        CHECK(feature.field("Kind").stringValue() == "primary");
        CHECK(feature.geometry.points.size() == 2);
        {
            auto iterator = source->iterate(context.get()->io);
            REQUIRE(iterator.hasMore());
            auto iterated = iterator.next();
            CHECK(iterated.hasField("Kind"));
            CHECK(!iterated.hasField("kind"));
            CHECK(iterated.field("Kind").stringValue() == "primary");
        }
        auto parallel = std::async(std::launch::async, [&] { return source->query(query, context.get()->io); });
        REQUIRE(source->query(query, context.get()->io).ok());
        REQUIRE(parallel.get().ok());
        query.maxVertices = 1;
        CHECK(source->query(query, context.get()->io).failed());
        query.maxVertices = 100;
        query.extent = GeoExtent(SRS::SPHERICAL_MERCATOR,5000000,5000000,6000000,6000000);
        auto empty = source->query(query, context.get()->io);
        REQUIRE(empty.ok());
        CHECK(empty.value()->records.empty());
        source->close();
    }
}

#ifdef ROCKY_HAS_SLUGHORN
TEST_CASE("Generic datasets and XYZ FGB compile through the same terrain stroke path", "[styled-features]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    files.create("0-0-0.fgb", "FlatGeobuf");
    files.create("roads.shp", "ESRI Shapefile");
    auto io = context.get()->io.from((files.directory / "map.json").generic_string());
    std::vector<json> bindings = {
        {{"type","GDAL"},{"uri","0-0-0.fgb"}},
        {{"type","GDAL"},{"uri","roads.shp"}},
        {{"type","TiledFeatures"},{"format","FlatGeobuf"},{"tiles",{"{z}-{x}-{y}.fgb"}},
         {"maxZoom",0},{"profile","spherical-mercator"}}
    };
    for (const auto& binding : bindings)
    {
        INFO(binding.dump());
        json config = {{"style",roadStyle()}, {"sourceBindings",{{"roads",binding}}}, {"styleZoom",4}};
        auto layer = StyledFeatureLayer::create(config.dump(), io);
        auto opened = layer->open(io);
        INFO((opened.failed() ? opened.error().message : "opened"));
        REQUIRE(opened.ok());
        for (const char* profile : {"spherical-mercator", "global-geodetic", "plate-carree"})
        {
            auto result = layer->createTile(TileKey(0,0,0,Profile(profile)), io);
            INFO((result.failed() ? result.error().message : "created"));
            REQUIRE(result.ok());
            REQUIRE(!result->shapes.empty());
            CHECK(result->validate().ok());
            CHECK(result->shapes.front().color.g == Approx(1.0));
        }
        CHECK(layer->stats().gpuStrokePieces > 0);
    }
}

TEST_CASE("Generic geometry preserves polygon holes and reprojects a non-Mercator dataset", "[styled-features]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    auto path = files.create("polygons.fgb", "FlatGeobuf", true, 4326,
        "POLYGON ((-10 -10,10 -10,10 10,-10 10,-10 -10),(-5 -5,-5 5,5 5,5 -5,-5 -5))");
    auto style = roadStyle();
    style["layers"][0]["type"] = "fill";
    style["layers"][0].erase("layout");
    style["layers"][0]["paint"] = {{"fill-color","#00ff00"}};
    json config = {{"style",style},{"sourceBindings",{{"roads",{{"type","GDAL"},{"uri",path}}}}}};
    auto layer = StyledFeatureLayer::create(config.dump(), context.get()->io);
    REQUIRE(layer->open(context.get()->io).ok());
    auto result = layer->createTile(TileKey(0,0,0,Profile("spherical-mercator")), context.get()->io);
    INFO((result.failed() ? result.error().message : "created"));
    REQUIRE(result.ok());
    REQUIRE(!result->shapes.empty());
    bool holes = false;
    for (const auto& shape : result->shapes)
        for (const auto& polygon : shape.polygons) holes |= polygon.size() == 2;
    CHECK(holes);
}

TEST_CASE("Generic features cover polar projected destinations outside the Mercator domain", "[styled-features]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    auto path = files.create("polar.fgb", "FlatGeobuf", true, 4326, "LINESTRING (-30 88, 30 88.5)");
    json config = {{"style",roadStyle()},{"styleZoom",8},
        {"sourceBindings",{{"roads",{{"type","GDAL"},{"uri",path}}}}}};
    auto layer = StyledFeatureLayer::create(config.dump(), context.get()->io);
    REQUIRE(layer->open(context.get()->io).ok());
    SRS srs("+proj=stere +lat_0=90 +lat_ts=70 +lon_0=0 +datum=WGS84 +units=m +no_defs");
    Profile profile(srs, Box(-500000,-500000,500000,500000), 1, 1);
    auto result = layer->createTile(TileKey(0,0,0,profile), context.get()->io);
    INFO((result.failed() ? result.error().message : "created"));
    REQUIRE(result.ok());
    REQUIRE(!result->shapes.empty());
    CHECK(result->validate().ok());
    CHECK(layer->stats().gpuStrokePieces > 0);
}

TEST_CASE("Tiled FGB uses its declared profile and deduplicates whole features", "[styled-features]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    files.create("0-0-0.fgb", "FlatGeobuf", true, 4326, "LINESTRING (-20 -2,20 2)");
    files.create("0-1-0.fgb", "FlatGeobuf", true, 4326, "LINESTRING (-20 -2,20 2)");
    auto source = TiledFeatureSource::create();
    source->tiles = {URI((files.directory / "{z}-{x}-{y}.fgb").generic_string())};
    source->profile = Profile("global-geodetic");
    source->maxZoom = 0;
    source->wholeFeatures = true;
    source->idField = "Kind";
    FeatureSource::Query query;
    query.extent = GeoExtent(SRS::WGS84,-40,-10,40,10);
    auto result = source->query(query, context.get()->io);
    INFO((result.failed() ? result.error().message : "queried"));
    REQUIRE(result.ok());
    REQUIRE(result.value()->records.size() == 1);
    CHECK(!result.value()->records.front().ownership.valid());
}

TEST_CASE("Tiled FGB distinguishes row schemes and rejects missing or excessive source requests", "[styled-features]")
{
    auto context = ContextFactory::create();
    FeatureFiles files;
    files.create("1-0-1.fgb", "FlatGeobuf", true, 4326, "LINESTRING (-100 20,-90 25)");
    auto source = TiledFeatureSource::create();
    source->tiles = {URI((files.directory / "{z}-{x}-{y}.fgb").generic_string())};
    source->minZoom = source->maxZoom = 1;
    source->tms = true;
    FeatureSource::Query query;
    query.extent = GeoExtent(SRS::WGS84,-110,15,-80,30);
    auto result = source->query(query, context.get()->io);
    REQUIRE(result.ok());
    REQUIRE(result.value()->records.size() == 1);
    CHECK(result.value()->records.front().ownership.ymin() == Approx(0));
    source->tms = false;
    CHECK(source->query(query, context.get()->io).failed()); // wrong row is a genuine missing tile
    source->minZoom = source->maxZoom = 14;
    query.extent = GeoExtent(SRS::WGS84,-180,-80,180,80);
    CHECK(source->query(query, context.get()->io).failed()); // reject before unbounded I/O
}
#endif
#endif
