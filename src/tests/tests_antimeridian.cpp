/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/GeoImage.h>
#include <rocky/ImageLayer.h>
#include <rocky/Profile.h>
#include <rocky/TileKey.h>
#include <cmath>
#include <vector>

using namespace ROCKY_NAMESPACE;

namespace
{
    // Exact source definition from https://github.com/pelicanmapping/rocky/issues/229.
    constexpr const char* legacyLongLat = "+proj=longlat +ellps=WGS84 +towgs84=0,0,0,0,0,0,0 +no_defs";

    //! A periodic longitude signal makes a seam or a clamped column visible without an external raster dataset.
    float longitudeSignal(double longitude)
    {
        return static_cast<float>(0.5 + 0.25 * std::sin(glm::radians(longitude)));
    }

    //! Generates small, deterministic source tiles for the real CPU reprojection path; no files or GPU are required.
    class AntimeridianImageLayer : public ImageLayer
    {
    public:
        //! Use the supplied source CRS and disable sharpening so the test measures only resampling.
        explicit AntimeridianImageLayer(const Profile& source)
        {
            profile = source;
            tileSize = 17u;
            sharpness = -1.0f;
        }

    protected:
        //! Return a newly owned tile whose endpoint samples match GeoImage's normalized sampling convention.
        Result<GeoImage> createTileImplementation(const TileKey& key, const IOOptions&) const override
        {
            auto image = Image::create(Image::R32_SFLOAT, tileSize.value(), tileSize.value());
            const auto extent = key.extent();
            for (unsigned row = 0; row < image->height(); ++row)
                for (unsigned col = 0; col < image->width(); ++col)
                {
                    const double longitude = extent.xmin() + extent.width() * col / (image->width() - 1);
                    image->write({ longitudeSignal(longitude), 0.0f, 0.0f, 1.0f }, col, row);
                }
            return GeoImage(image, extent);
        }
    };
}

// Run passing coverage with "[issue229]~[issue229-repro]".
// Known failures are hidden from the default suite; select "[issue229-repro]" to run them as ordinary assertions.
// Do not use !shouldfail: a repaired behavior should pass, and unrelated assertion failures must remain visible.

//! Compare scalar and bulk QSC transforms across the seam without involving extent or raster coordinate handling.
TEST_CASE("Issue 229 QSC point transforms agree with global-geodetic", "[srs][antimeridian][issue229]")
{
    const Profile legacy(legacyLongLat);
    const Profile geodetic("global-geodetic");
    REQUIRE(legacy.valid());
    REQUIRE(geodetic.valid());
    INFO("PROJ " << SRS::projVersion());

    struct Face { const char* name; double latitude; };
    for (const auto& face : { Face{ "qsc-x", 20.0 }, Face{ "qsc+z", 70.0 }, Face{ "qsc-z", -70.0 } })
    {
        const Profile qsc(face.name);
        REQUIRE(qsc.valid());
        const auto reference = geodetic.srs().to(qsc.srs());
        const auto forward = legacy.srs().to(qsc.srs());
        const auto backward = qsc.srs().to(legacy.srs());
        REQUIRE(reference.valid());
        REQUIRE(forward.valid());
        REQUIRE(backward.valid());
        INFO("face: " << face.name << ", legacy pipeline: " << forward.string());

        std::vector<glm::dvec3> input;
        for (double longitude : { 170.0, 179.999, 180.0, -180.0, -179.999, -170.0, 190.0 })
            input.emplace_back(longitude, face.latitude, 0.0);
        auto projected = input;
        REQUIRE(forward.transformArray(projected.data(), projected.size()));
        auto restored = projected;
        REQUIRE(backward.transformArray(restored.data(), restored.size()));
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            CAPTURE(input[i].x);
            CAPTURE(input[i].y);
            glm::dvec3 expected, scalar, inverse;
            REQUIRE(reference.transform(input[i], expected));
            REQUIRE(forward.transform(input[i], scalar));
            REQUIRE(forward.inverse(projected[i], inverse));
            CHECK(glm::length(scalar - expected) < 1e-5); // metres
            CHECK(glm::length(projected[i] - expected) < 1e-5);
            CHECK(std::abs(std::remainder(restored[i].x - input[i].x, 360.0)) < 1e-7); // degrees
            CHECK(std::abs(restored[i].y - input[i].y) < 1e-7);
            CHECK(std::abs(std::remainder(inverse.x - input[i].x, 360.0)) < 1e-7);
            CHECK(std::abs(inverse.y - input[i].y) < 1e-7);
        }
        // The seam endpoints coincide, and adjacent points are at most a kilometre apart on each face.
        CHECK(glm::length(projected[2] - projected[3]) < 1e-5);
        CHECK(glm::length(projected[1] - projected[4]) < 1000.0);
    }
}

//! Reproduce the legacy bound CRS losing the longitude span used to clamp QSC samples during tile assembly.
TEST_CASE("Issue 229 legacy geographic metadata and extents", "[.][antimeridian][issue229][issue229-repro]")
{
    const SRS legacy(legacyLongLat);
    const Profile geodetic("global-geodetic");
    const Profile qsc("qsc-x");
    REQUIRE(legacy.valid());
    REQUIRE(qsc.valid());
    INFO("PROJ " << SRS::projVersion());
    CHECK(legacy.isGeodetic());

    const auto expected = qsc.extent().transform(geodetic.srs());
    const auto actual = qsc.extent().transform(legacy);
    INFO("global-geodetic bounds: " << expected.toString() << ", legacy bounds: " << actual.toString());
    REQUIRE(expected.valid());
    REQUIRE(actual.valid());
    CHECK(expected.width() == Approx(90.0));
    CHECK(expected.crossesAntimeridian());
    CHECK(actual.width() == Approx(90.0));
    CHECK(actual.crossesAntimeridian());
    for (double longitude : { 170.0, 179.999, -180.0, -179.999, -170.0 })
    {
        CAPTURE(longitude);
        CHECK(actual.contains(longitude, 0.0));
    }
}

namespace
{
    //! Exercise raster unwrapping (69a4b6bf) and interval containment/expansion (3a985234) for a source profile.
    void checkCrossingRasterAndExtents(const char* source)
    {
        const Profile profile(source);
        CAPTURE(source);
        REQUIRE(profile.valid());
        const GeoExtent extent(profile.srs(), 170.0, -10.0, 190.0, 10.0);
        Heightfield heights(5, 3);
        for (unsigned row = 0; row < heights.height(); ++row)
            for (unsigned col = 0; col < heights.width(); ++col)
                heights.heightAt(col, row) = 10.0f * col + row;
        const GeoImage geo(heights.image, extent);
        const GeoHeightfield hf(geo);
        REQUIRE(geo.valid());
        const Profile qsc("qsc-x");
        const auto toQSC = SRS::WGS84.to(qsc.srs());
        REQUIRE(toQSC.valid());
        for (double longitude : { 175.0, 180.0, -180.0, -175.0, 185.0 })
        {
            CAPTURE(longitude);
            const double unwrapped = longitude < 0.0 ? longitude + 360.0 : longitude;
            const double expected = 2.0 * (unwrapped - 170.0) + 1.0;
            glm::dvec3 projected;
            REQUIRE(toQSC.transform(glm::dvec3(longitude, 0.0, 0.0), projected));
            const auto direct = geo.read(longitude, 0.0);
            const auto reprojected = geo.read(GeoPoint(qsc.srs(), projected.x, projected.y));
            const auto height = hf.read(longitude, 0.0);
            CHECK(direct.ok());
            CHECK(reprojected.ok());
            CHECK(height.ok());
            if (direct.ok()) CHECK(direct->r == Approx(expected));
            if (reprojected.ok()) CHECK(reprojected->r == Approx(expected));
            if (height.ok()) CHECK(height.value() == Approx(expected));
        }
        const GeoExtent world(profile.srs(), -180.0, -90.0, 180.0, 90.0);
        const GeoExtent hemisphere(profile.srs(), 0.0, -90.0, 180.0, 90.0);
        CHECK_FALSE(hemisphere.contains(world));
        auto expanded = extent;
        REQUIRE(expanded.expandToInclude(world));
        CHECK(expanded.width() == Approx(360.0));
        CHECK(world.contains(extent));
        CHECK(expanded.contains(world));
    }

    //! Assemble both sides of the actual antimeridian QSC face and check every pixel against an independent signal.
    void checkQSCTileAssembly(const char* source)
    {
        CAPTURE(source);
        INFO("PROJ " << SRS::projVersion());
        AntimeridianImageLayer layer{ Profile(source) };
        const IOOptions io;
        REQUIRE(layer.open(io).ok());
        const Profile qsc("qsc-x");
        REQUIRE(qsc.valid());
        const auto toGeodetic = qsc.srs().to(SRS::WGS84);
        REQUIRE(toGeodetic.valid());
        const auto keys = qsc.rootKeys();
        REQUIRE(keys.size() == 4u);
        for (const auto& key : keys)
        {
            INFO("tile: " << key.str());
            const auto tile = layer.createTile(key, io);
            REQUIRE(tile.ok());
            REQUIRE(tile.value().valid());
            const auto image = tile.value().image();
            const auto extent = key.extent();
            INFO("source bounds: " << extent.transform(layer.profile.srs()).toString());
            unsigned incorrectSamples = 0;
            float maximumError = 0.0f;
            for (unsigned row = 0; row < image->height(); ++row)
                for (unsigned col = 0; col < image->width(); ++col)
                {
                    glm::dvec3 location(
                        extent.xmin() + extent.width() * (col + 0.5) / image->width(),
                        extent.ymin() + extent.height() * (row + 0.5) / image->height(), 0.0);
                    REQUIRE(toGeodetic.transform(location, location));
                    // Allow interpolation error in a 17-column source tile, but not a repeated seam column.
                    const float expected = longitudeSignal(location.x);
                    const float actual = image->read(col, row).r;
                    const float error = std::abs(actual - expected);
                    if (!(error < 0.002f)) // Also count NaNs as incorrect.
                        ++incorrectSamples;
                    maximumError = std::max(maximumError, error);
                }
            // Aggregate mismatches so a blank tile gives one diagnostic instead of hundreds.
            CAPTURE(maximumError);
            CHECK(incorrectSamples == 0u);
        }
    }
}

//! The documented workaround and an unbound longlat CRS must retain the behavior of both September fixes.
TEST_CASE("Issue 229 crossing raster and extent controls", "[geoimage][geoextent][antimeridian][issue229]")
{
    SECTION("global-geodetic workaround")
    {
        checkCrossingRasterAndExtents("global-geodetic");
    }
    SECTION("unbound longlat control")
    {
        checkCrossingRasterAndExtents("+proj=longlat +ellps=WGS84 +no_defs");
    }
}

//! These positive expectations stay hidden until bound geographic CRS metadata enables longitude handling.
TEST_CASE("Issue 229 legacy crossing raster and extent regression", "[.][antimeridian][issue229][issue229-repro]")
{
    checkCrossingRasterAndExtents(legacyLongLat);
}

//! Protect the issue's global-geodetic workaround through source tile selection, bounds, and raster sampling.
TEST_CASE("Issue 229 global-geodetic QSC tile assembly", "[imagelayer][antimeridian][issue229]")
{
    checkQSCTileAssembly("global-geodetic");
}

//! The same synthetic dataset with the original legacy CRS must yield the same longitude signal at the seam.
TEST_CASE("Issue 229 legacy QSC tile assembly regression", "[.][antimeridian][issue229][issue229-repro]")
{
    checkQSCTileAssembly(legacyLongLat);
}
