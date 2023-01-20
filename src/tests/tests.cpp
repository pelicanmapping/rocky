#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/Instance.h>
#include <rocky/Map.h>
#include <rocky/Math.h>
#include <rocky/Notify.h>
#include <rocky/Image.h>
#include <rocky/Heightfield.h>
#include <rocky/GDALLayers.h>
#include <rocky/URI.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    class TestLayer : public Inherit<Layer, TestLayer>
    {
    public:
        Status openImplementation(const IOOptions& io) override {
            return StatusOK;
        }
    };
}

TEST_CASE("Math")
{
    CHECK(is_identity(fmat4(1)));
    CHECK(!is_identity(fmat4()));

    fmat4 scale_bias{ 1 };
    scale_bias = glm::translate(fmat4(1), fvec3(0.25, 0.25, 0.0));
    scale_bias = glm::scale(scale_bias, fvec3(0.5, 0.5, 1.0));
    CHECK(!is_identity(scale_bias));
    CHECK(scale_bias[0][0] == 0.5f);
    CHECK(scale_bias[1][1] == 0.5f);
    CHECK(scale_bias[3][0] == 0.25f);
    CHECK(scale_bias[3][1] == 0.25f);

    fvec3 r = scale_bias * fvec3(1, 1, 0);
    CHECK(r == fvec3(0.75f, 0.75f, 0));
}

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
    image->fill(Color(1, 0.5, 0.0, 1));
    Image::Pixel value;
    image->read(value, 17, 17);
    //std::cout << value.r << ", " << value.g << ", " << value.b << ", " << value.a << std::endl;
    CHECK(equiv(value.r, 1.0f, 0.01f));
    CHECK(equiv(value.g, 0.5f, 0.01f));
    CHECK(equiv(value.b, 0.0f, 0.01f));
    CHECK(equiv(value.a, 1.0f, 0.01f));
}

TEST_CASE("Heightfield")
{
    auto hf = Heightfield::create(257, 257);
    REQUIRE(hf);
    if (hf) {
        // test metadata:
        CHECK(hf->pixelFormat() == Image::R32_SFLOAT);
        CHECK(hf->numComponents() == 1);
        CHECK(hf->sizeInBytes() == 264196);
        CHECK(hf->rowSizeInBytes() == 1028);
        CHECK(hf->componentSizeInBytes() == 4);
        CHECK(hf->sizeInPixels() == 66049);

        // write/read:
        hf->heightAt(16, 16) = 100.0f;
        hf->heightAt(16, 17) = 50.0f;
        hf->heightAt(17, 16) = 50.0f;
        hf->heightAt(17, 17) = 100.0f;
        CHECK(hf->heightAt(16, 16) == 100.0f);
        CHECK(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == 75.0f);
        
        // read with NO_DATA_VALUEs:
        hf->heightAt(17, 17) = NO_DATA_VALUE;
        hf->heightAt(16, 16) = NO_DATA_VALUE;
        CHECK(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == 50.0f);

        // all NODATA:
        hf->fill(NO_DATA_VALUE);
        CHECK(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == NO_DATA_VALUE);
    }
}

TEST_CASE("Map")
{
    auto instance = Instance::create();

    auto map = Map::create(instance);
    REQUIRE(map);
    if (map) {
        auto layer = TestLayer::create();

        map->addLayer(layer);
        CHECK(map->numLayers() == 1);

        map->removeLayer(layer);
        CHECK(map->numLayers() == 0);
    }
}

TEST_CASE("Open Layer")
{
#ifdef GDAL_FOUND
    SECTION("GDAL")
    {
        auto layer = GDALImageLayer::create();
        CHECKED_IF(layer != nullptr)
        {
            layer->setName("World imagery");
            layer->setURI("D:/data/imagery/world.tif");
            auto s = layer->open();
            CHECK(s.ok());
        }
    }
#else
    INFO("GDAL not avaiable - skipping GDAL tests");
#endif
}

TEST_CASE("Deserialize layer")
{
#ifdef GDAL_FOUND
    auto instance = Instance::create();
    Config conf("gdalimage");
    conf.set("name", "World imagery");
    conf.set("url", "D:/data/imagery/world.tif");
    auto layer = Layer::cast(instance->read(conf));
    CHECKED_IF(layer != nullptr)
    {
        auto s = layer->open();
        CHECK(s.ok());
        if (s.failed()) ROCKY_WARN << s.toString() << std::endl;
    }
#endif
}

TEST_CASE("SRS")
{
    // See info messages from the SRS system
    Instance::log().threshold = LogThreshold::INFO;

    // epsilon
    const double E = 0.1;

    SECTION("Spherical Mercator <> Geographic")
    {        
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());
        CHECK(merc.isProjected() == true);
        CHECK(merc.isGeographic() == false);
        CHECK(merc.isGeocentric() == false);

        SRS wgs84("epsg:4326"); // geographic WGS84 (long/lat/hae)
        REQUIRE(wgs84.valid());
        CHECK(wgs84.isProjected() == false);
        CHECK(wgs84.isGeographic() == true);
        CHECK(wgs84.isGeocentric() == false);

        auto xform = merc.to(wgs84);
        REQUIRE(xform.valid());

        dvec3 out;
        REQUIRE(xform(dvec3(-20037508.34278925, 0, 0), out));
        CHECK(equiv(out, dvec3(-180, 0, 0), E));
        
        // NB: succeeds despite the 90 degrees N being out of bounds for Mercator.
        CHECK(xform.inverse(dvec3(0, 90, 0), out));
        CHECK(out.y > merc.bounds().ymax);
    }

    SECTION("Geographic SRS")
    {
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());

        SRS geo = merc.geoSRS();
        REQUIRE(geo.valid());
        CHECK(geo.isGeographic());

        SRS utm("epsg:32632"); // UTM32/WGS84
        CHECK(utm.geoSRS().isGeographic());
    }

    SECTION("Geographic <> Geocentric")
    {
        SRS wgs84("epsg:4326"); // geographic WGS84 (long/lat/hae)
        REQUIRE(wgs84.valid());
        CHECK(wgs84.isProjected() == false);
        CHECK(wgs84.isGeographic() == true);
        CHECK(wgs84.isGeocentric() == false);

        SRS ecef("epsg:4978"); // geocentric WGS84 (ECEF)
        REQUIRE(ecef.valid());
        CHECK(ecef.isProjected() == false);
        CHECK(ecef.isGeographic() == false);
        CHECK(ecef.isGeocentric() == true);

        dvec3 out;

        auto xform_wgs84_to_ecef = wgs84.to(ecef);
        REQUIRE(xform_wgs84_to_ecef.valid());

        REQUIRE(xform_wgs84_to_ecef(dvec3(0, 0, 0), out));
        CHECK(equiv(out, dvec3(6378137, 0, 0)));

        REQUIRE(xform_wgs84_to_ecef.inverse(out, out));
        CHECK(equiv(out, dvec3(0, 0, 0)));
    }

    SECTION("Plate Carree SRS")
    {
        auto pc = SRS("plate-carree");
        REQUIRE(pc == SRS::PLATE_CARREE);
        CHECK(pc.isProjected() == true);
        CHECK(pc.isGeographic() == false);
        CHECK(pc.isGeocentric() == false);
        auto b = pc.bounds();
        CHECK((b.valid() &&
            equiv(b.xmin, -20037508.342, E) && equiv(b.xmax, 20037508.342, E) &&
            equiv(b.ymin, -10018754.171, E) && equiv(b.ymax, 10018754.171, E)));
    }

    SECTION("UTM SRS")
    {
        SRS utm32N("epsg:32632"); // +proj=utm +zone=32 +datum=WGS84
        REQUIRE(utm32N.valid());
        CHECK(utm32N.isProjected() == true);
        CHECK(utm32N.isGeographic() == false);
        CHECK(utm32N.isGeocentric() == false);
        CHECK(utm32N.bounds().valid());

        SRS utm32S("+proj=utm +zone=32 +south +datum=WGS84");
        REQUIRE(utm32S.valid());
        CHECK(utm32S.isProjected() == true);
        CHECK(utm32S.isGeographic() == false);
        CHECK(utm32S.isGeocentric() == false);
        auto b = utm32S.bounds();
        CHECK((b.valid() && b.xmin == 166000 && b.xmax == 834000 && b.ymin == 1116915 && b.ymax == 10000000));
    }

    SECTION("Quadrilateralized Spherical Cube SRS")
    {
        double E = 1.0;

        SRS wgs84("wgs84");
        REQUIRE(wgs84.valid());

        SRS qsc0("+wktext +proj=qsc +units=m +ellps=WGS84 +datum=WGS84 +lat_0=0 +lon_0=0");
        REQUIRE(qsc0.valid());
        auto qsc_bounds = qsc0.bounds();
        CHECK(qsc_bounds.valid());

        auto xform = wgs84.to(qsc0);
        REQUIRE(xform.valid());

        double semi_major = wgs84.ellipsoid().semiMajorAxis();
        double semi_minor = wgs84.ellipsoid().semiMinorAxis();

        dvec3 c;
        REQUIRE(xform(dvec3(0, 0, 0), c));
        CHECK(equiv(c, dvec3(0, 0, 0), E));
        // long and lat are out of range for face 0, but doesn't fail
        //CHECK(xform(dvec3(90, 46, 0), c) == false);

        REQUIRE(xform(dvec3(45, 0, 0), c));
        CHECK(equiv(c, dvec3(semi_major, 0, 0), E));

        REQUIRE(xform.inverse(dvec3(semi_major, 0, 0), c));
        CHECK(equiv(c, dvec3(45, 0, 0), E));

        REQUIRE(xform(dvec3(0, 45, 0), c));
        // FAILS - not sure what is up here:
        // 45 degrees transforms to 6352271.2440m
        // but the semi-minor axis is 6356752.3142m
        //CHECK(equiv(c, dvec3(0, semi_minor, 0), E));

        // other way
        xform = qsc0.to(wgs84);
        REQUIRE(xform.valid());

        REQUIRE(xform(dvec3(semi_major, 0, 0), c));
        CHECK(equiv(c, dvec3(45, 0, 0), E));
    }

    SECTION("Invalid SRS")
    {
        SRS bad("gibberish");
        CHECK(bad.valid() == false);
        CHECK(bad.isProjected() == false);
        CHECK(bad.isGeographic() == false);
        CHECK(bad.isGeocentric() == false);
    }

    SECTION("SRS with Vertical Datum")
    {
        SRS wgs84("epsg:4326"); // geographic WGS84
        REQUIRE(wgs84.valid());

        SRS egm96("epsg:4326", "us_nga/us_nga_egm96_15"); // WGS84 with EGM96 vdatum
        REQUIRE(egm96.valid());

        // EGM96 test values are from:
        // http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm96/intpt.html
        dvec3 out;

        // geodetic to vdatum:
        {
            auto xform = wgs84.to(egm96);
            REQUIRE(xform.valid());

            REQUIRE(xform(dvec3(0, 0, 17.16), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(90, 0, -63.24), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(180, 0, 21.15), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(-90, 0, -4.29), out));
            CHECK(equiv(out.z, 0.0, E));

            // inverse
            REQUIRE(xform.inverse(dvec3(0, 0, 0), out));
            CHECK(equiv(out.z, 17.16, E));
            REQUIRE(xform.inverse(dvec3(90, 0, 0), out));
            CHECK(equiv(out.z, -63.24, E));
            REQUIRE(xform.inverse(dvec3(180, 0, 0), out));
            CHECK(equiv(out.z, 21.15, E));
            REQUIRE(xform.inverse(dvec3(-90, 0, 0), out));
            CHECK(equiv(out.z, -4.29, E));
        }

        // vdatum to geodetic:
        {
            auto xform = egm96.to(wgs84);
            REQUIRE(xform.valid());

            REQUIRE(xform(dvec3(0, 0, 0), out));
            CHECK(equiv(out.z, 17.16, E));
            REQUIRE(xform(dvec3(90, 0, 0), out));
            CHECK(equiv(out.z, -63.24, E));
            REQUIRE(xform(dvec3(180, 0, 0), out));
            CHECK(equiv(out.z, 21.15, E));
            REQUIRE(xform(dvec3(-90, 0, 0), out));
            CHECK(equiv(out.z, -4.29, E));

            // inverse
            REQUIRE(xform.inverse(dvec3(0, 0, 17.16), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(90, 0, -63.24), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(180, 0, 21.15), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(-90, 0, -4.29), out));
            CHECK(equiv(out.z, 0.0, E));
        }

        // vdatum to vdatum (noop)
        {
            auto xform = egm96.to(egm96);
            REQUIRE(xform.valid());

            dvec3 out;
            REQUIRE(xform(dvec3(0, 0, 17.16), out));
            CHECK(equiv(out.z, 17.16, E));
        }
    }

    SECTION("SRS Metadata")
    {
        Box b;
        b = SRS::WGS84.bounds();
        CHECK(equiv(b.xmin, -180.0));
        CHECK(equiv(b.xmax, 180.0));
        CHECK(equiv(b.ymin, -90.0));
        CHECK(equiv(b.ymax, 90.0));

        b = SRS::SPHERICAL_MERCATOR.bounds();
        CHECK(equiv(b.xmin, -20037508.342789244, E));
        CHECK(equiv(b.xmax, 20037508.342789244, E));
        CHECK(equiv(b.ymin, -20048966.104014594, E));
        CHECK(equiv(b.ymax, 20048966.104014594, E));

        auto ellipsoid = SRS::WGS84.ellipsoid();
        REQUIRE(ellipsoid.semiMajorAxis() == 6378137.0);

        Units units;
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
            dvec3 out;
            REQUIRE(xform(dvec3(-180, 0, 0), out));
            CHECK(equiv(out, dvec3(-20037508.34278925, 0, 0)));
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

        std::vector<TileKey> keys;
        Profile::getRootKeys(GG, keys);
        REQUIRE(keys.size() == 2);
        CHECK(keys[0] == TileKey(0, 0, 0, GG));
        CHECK(keys[1] == TileKey(0, 1, 0, GG));
    }
}

TEST_CASE("IO")
{
    SECTION("HTTP")
    {
        URI uri("http://readymap.org/readymap/tiles/1.0.0/7/");
        auto r = uri.read(nullptr);
        CHECKED_IF(r.status.ok())
        {
            CHECK(r.value.contentType == "text/xml");

            auto body = r.value.data.to_string();
            CHECK(!body.empty());
            CHECK(rocky::util::startsWith(body, "<?xml"));
        }
        else
        {
            std::cerr << "HTTP/S request failed: " << r.status.message << std::endl;
        }
    }

    SECTION("HTTPS")
    {
        if (URI::supportsHTTPS())
        {
            URI uri("https://readymap.org/readymap/tiles/1.0.0/7/");
            auto r = uri.read(nullptr);
            CHECKED_IF(r.status.ok())
            {
                CHECK(r.value.contentType == "text/xml");
                auto body = r.value.data.to_string();
                CHECK(!body.empty());
                CHECK(rocky::util::startsWith(body, "<?xml"));
            }
        }
        else
        {
            WARN("HTTPS support is not available - skipping HTTP tests");
        }
    }
}