/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "catch.hpp"
#include <rocky/FileContentCache.h>
#include <rocky/URI.h>
#include <rocky/vsg/MapNode.h>
#include <rocky/vsg/Application.h>
#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>
#include <fstream>
#include <future>

using namespace ROCKY_NAMESPACE;
namespace
{
    //! Owns only a freshly created test directory and cleans it up after cache handles are released.
    struct CacheFiles
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("rocky-content-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        //! Creates this test's isolated workspace.
        CacheFiles() { std::filesystem::create_directories(path); }
        //! Removes this test's files, ignoring cleanup errors.
        ~CacheFiles() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    };
}

TEST_CASE("Content cache persists binary payloads and rejects corrupt records", "[content-cache]")
{
    CacheFiles files;
    const std::string key = "https://example.invalid/tiles/1.pbf?x=2";
    Content original;
    original.type = "application/vnd.mapbox-vector-tile";
    original.data = std::string("a\0b\xff", 4);
    original.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(123456789));
    {
        FileContentCache cache(files.path.string(), 1);
        cache.put(key, original);
        cache.put("other", Content{});
        CHECK(cache.size() == 1);
        auto hit = cache.get(key);
        REQUIRE(hit.has_value());
        CHECK(hit->value().data == original.data);
    }
    FileContentCache disk(files.path.string(), 0);
    auto hit = disk.get(key);
    REQUIRE(hit.has_value());
    CHECK(hit->value().type == original.type);
    CHECK(hit->value().data == original.data);
    CHECK(hit->value().timestamp == original.timestamp);
    CHECK(disk.hits() == 1);
    CHECK(!disk.get("missing").has_value());
    CHECK(disk.misses() == 1);
    disk.put("failure", Failure_ResourceUnavailable);
    CHECK(!disk.get("failure").has_value());

    // Alter same-sized payloads so integrity checking, rather than only length checking, detects damage.
    for (const auto& file : std::filesystem::directory_iterator(files.path / "rocky-content-v1"))
    {
        std::fstream out(file.path(), std::ios::in | std::ios::out | std::ios::binary);
        out.seekp(-1, std::ios::end);
        out.put('!');
    }
    CHECK(!disk.get(key).has_value());
    disk.put(key, original);
    REQUIRE(disk.get(key).has_value());
    { std::ofstream keep(files.path / "keep.txt"); keep << "unrelated"; }
    { std::ofstream keep(files.path / "rocky-content-v1" / "keep.txt"); keep << "unrelated"; }
    disk.clear();
    CHECK(!disk.get(key).has_value());
    CHECK(std::filesystem::exists(files.path / "keep.txt"));
    CHECK(std::filesystem::exists(files.path / "rocky-content-v1" / "keep.txt"));
}

TEST_CASE("URI uses the shared persistent cache across service instances", "[content-cache]")
{
    CacheFiles files;
    const auto source = files.path / "source.bin";
    { std::ofstream out(source, std::ios::binary); out << "first response"; }
    IOOptions io;
    io.services().contentCache = std::make_shared<FileContentCache>((files.path / "cache").string(), 0);
    auto first = URI(source.generic_string()).read(io);
    REQUIRE(first.ok());
    CHECK(!first->fromCache);
    std::filesystem::remove(source);
    IOOptions reopened;
    reopened.services().contentCache = std::make_shared<FileContentCache>((files.path / "cache").string(), 0);
    auto second = URI(source.generic_string()).read(reopened);
    REQUIRE(second.ok());
    CHECK(second->fromCache);
    CHECK(second->content.data == "first response");
    CHECK(second->content.timestamp == std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::milliseconds>(first->content.timestamp.time_since_epoch())));

    std::vector<std::future<bool>> reads;
    for (unsigned i = 0; i < 8; ++i)
        reads.emplace_back(std::async(std::launch::async, [&]() {
            auto result = URI(source.generic_string()).read(reopened);
            return result.ok() && result->fromCache && result->content.data == "first response";
        }));
    for (auto& read : reads) CHECK(read.get());
}

TEST_CASE("Map files round trip shared content cache configuration", "[content-cache][serialization]")
{
    CacheFiles files;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    auto node = MapNode::create(context.get());
    json config = {
        { "services", { { "contentCache", { { "directory", files.path.generic_string() }, { "memoryEntries", 3 } } } } },
        { "map", { { "layers", json::array() } } }
    };
    REQUIRE(node->from_json(config.dump(), context.get()->io).ok());
    auto cache = std::dynamic_pointer_cast<FileContentCache>(context.get()->io.services().contentCache);
    REQUIRE(cache);
    CHECK(cache->capacity() == 3);
    CHECK(cache->directory() == files.path.generic_string());
    auto serialized = json::parse(node->to_json());
    CHECK(serialized["services"] == config["services"]);
    REQUIRE(node->from_json(serialized.dump(), context.get()->io).ok());
    CHECK(context.get()->io.services().contentCache == cache); // unchanged settings retain resident entries
    REQUIRE(context.get()->io.services().from_json("{}").ok());
    CHECK(context.get()->io.services().contentCache == cache);
    for (const auto* invalid : { "[]", R"({"contentCache":null})", R"({"contentCache":{"directory":""}})",
        R"({"contentCache":{"directory":42}})", R"({"contentCache":{"directory":"x","memoryEntries":-1}})",
        R"({"contentCache":{"directory":"x","memoryEntries":1.5}})" })
    {
        CHECK(context.get()->io.services().from_json(invalid).failed());
        CHECK(context.get()->io.services().contentCache == cache);
    }
}

TEST_CASE("Application cache arguments override shared map configuration", "[content-cache]")
{
    CacheFiles files;
    const auto mapPath = files.path / "map.json";
    const auto mapCache = (files.path / "map-cache").generic_string();
    const auto cliCache = (files.path / "cli-cache").generic_string();
    json config = {
        { "services", { { "contentCache", { { "directory", mapCache } } } } },
        { "map", { { "layers", json::array() } } }
    };
    { std::ofstream out(mapPath); out << config.dump(); }
    {
        std::vector<std::string> args = { "cache-test", "--map", mapPath.generic_string(), "--cache", cliCache };
        std::vector<char*> argv;
        for (auto& arg : args) argv.push_back(arg.data());
        int argc = static_cast<int>(argv.size());
        argv.push_back(nullptr);
        Application app(argc, argv.data());
        REQUIRE(app.commandLineStatus.ok());
        const auto cache = std::dynamic_pointer_cast<FileContentCache>(app.io().services().contentCache);
        REQUIRE(cache);
        CHECK(cache->directory() == cliCache);
        const auto serialized = json::parse(app.mapNode->to_json());
        CHECK(serialized["services"]["contentCache"]["directory"] == cliCache);
    }
}
