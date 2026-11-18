/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "catch.hpp"
#include <rocky/URI.h>
#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>

using namespace ROCKY_NAMESPACE;

TEST_CASE("URI JSON preserves ordered header pairs", "[uri][serialization]")
{
    URI::Context context;
    context.headers = { { "X-Test", "first" }, { "X-Test", "second" }, { "Accept", "application/json" } };
    URI original("https://example.invalid/style.json", context);
    json encoded = original;
    REQUIRE(encoded.at("headers").is_array());
    CHECK(encoded.at("headers").at(0) == json::array({ "X-Test", "first" }));
    auto restored = encoded.get<URI>();
    CHECK(restored.full() == original.full());
    CHECK(restored.context().headers == original.context().headers);
    CHECK(json(restored) == encoded);
    CHECK(json(URI("relative.json")) == "relative.json");
}
