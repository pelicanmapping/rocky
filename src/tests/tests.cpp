#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/Instance.h>
#include <rocky/Map.h>
#include <rocky/Math.h>
#include <rocky/Notify.h>
#include <rocky/Image.h>
#include <rocky/Heightfield.h>
#include <rocky/GDALLayers.h>

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
    REQUIRE(is_identity(fmat4(1)));
    REQUIRE(!is_identity(fmat4()));

    fmat4 scale_bias{ 1 };
    scale_bias = glm::translate(fmat4(1), fvec3(0.25, 0.25, 0.0));
    scale_bias = glm::scale(scale_bias, fvec3(0.5, 0.5, 1.0));
    REQUIRE(!is_identity(scale_bias));
    REQUIRE(scale_bias[0][0] == 0.5f);
    REQUIRE(scale_bias[1][1] == 0.5f);
    REQUIRE(scale_bias[3][0] == 0.25f);
    REQUIRE(scale_bias[3][1] == 0.25f);

    fvec3 r = scale_bias * fvec3(1, 1, 0); // pre_mult(fvec3(0, 0, 0), scalebias);
    //std::cout << r.x << ", " << r.y << ", " << r.z << std::endl;
    REQUIRE(r == fvec3(0.75f, 0.75f, 0));


}

TEST_CASE("Image")
{
    auto image = Image::create(Image::R8G8B8A8_UNORM, 256, 256);
    REQUIRE(image);
    if (image) {
        REQUIRE(image->numComponents() == 4);
        REQUIRE(image->sizeInBytes() == 262144);
        REQUIRE(image->rowSizeInBytes() == 1024);
        REQUIRE(image->componentSizeInBytes() == 1);
        REQUIRE(image->sizeInPixels() == 65536);
    }

    auto clone = image->clone();
    REQUIRE(clone);
    if (clone) {
        REQUIRE(clone->numComponents() == 4);
        REQUIRE(clone->sizeInBytes() == 262144);
        REQUIRE(clone->rowSizeInBytes() == 1024);
        REQUIRE(clone->componentSizeInBytes() == 1);
        REQUIRE(clone->sizeInPixels() == 65536);
    }

    image = Image::create(Image::R8G8B8_UNORM, 256, 256);
    REQUIRE(image);
    if (image) {
        REQUIRE(image->numComponents() == 3);
        REQUIRE(image->sizeInBytes() == 196608);
        REQUIRE(image->rowSizeInBytes() == 768);
        REQUIRE(image->componentSizeInBytes() == 1);
        REQUIRE(image->sizeInPixels() == 65536);
    }

    image = Image::create(Image::R8G8B8A8_UNORM, 256, 256);
    image->fill(Color(1, 0.5, 0.0, 1));
    Image::Pixel value;
    image->read(value, 17, 17);
    //std::cout << value.r << ", " << value.g << ", " << value.b << ", " << value.a << std::endl;
    REQUIRE(equiv(value.r, 1.0f, 0.01f));
    REQUIRE(equiv(value.g, 0.5f, 0.01f));
    REQUIRE(equiv(value.b, 0.0f, 0.01f));
    REQUIRE(equiv(value.a, 1.0f, 0.01f));
}

TEST_CASE("Heightfield")
{
    auto hf = Heightfield::create(257, 257);
    REQUIRE(hf);
    if (hf) {
        // test metadata:
        REQUIRE(hf->pixelFormat() == Image::R32_SFLOAT);
        REQUIRE(hf->numComponents() == 1);
        REQUIRE(hf->sizeInBytes() == 264196);
        REQUIRE(hf->rowSizeInBytes() == 1028);
        REQUIRE(hf->componentSizeInBytes() == 4);
        REQUIRE(hf->sizeInPixels() == 66049);

        // write/read:
        hf->heightAt(16, 16) = 100.0f;
        hf->heightAt(16, 17) = 50.0f;
        hf->heightAt(17, 16) = 50.0f;
        hf->heightAt(17, 17) = 100.0f;
        REQUIRE(hf->heightAt(16, 16) == 100.0f);
        REQUIRE(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == 75.0f);
        
        // read with NO_DATA_VALUEs:
        hf->heightAt(17, 17) = NO_DATA_VALUE;
        hf->heightAt(16, 16) = NO_DATA_VALUE;
        REQUIRE(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == 50.0f);

        // all NODATA:
        hf->fill(NO_DATA_VALUE);
        REQUIRE(hf->heightAtPixel(16.5, 16.5, Heightfield::BILINEAR) == NO_DATA_VALUE);
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
        REQUIRE(map->numLayers() == 1);

        map->removeLayer(layer);
        REQUIRE(map->numLayers() == 0);
    }
}

TEST_CASE("Open layer")
{
    auto layer = GDALImageLayer::create();
    REQUIRE(layer);
    if (layer) {
        layer->setName("World imagery");
        layer->setURI("D:/data/imagery/world.tif");
        auto s = layer->open();
        REQUIRE(s.ok());
    }
}

TEST_CASE("Deserialize layer")
{
    auto instance = Instance::create();
    Config conf("gdalimage");
    conf.set("name", "World imagery");
    conf.set("url", "D:/data/imagery/world.tif");
    auto layer = Layer::cast(instance->read(conf));
    REQUIRE(layer);
    if (layer) {
        auto s = layer->open();
        REQUIRE(s.ok());
        if (s.failed()) ROCKY_WARN << s.toString() << std::endl;
    }
}

TEST_CASE("SRS")
{
    // See info messages from the SRS system
    Instance::log().threshold = LogThreshold::INFO;

    // epsilon
    const double E = 0.1;

    SECTION("spherical-mercator")
    {        
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());
        REQUIRE(merc.isProjected() == true);
        REQUIRE(merc.isGeographic() == false);
        REQUIRE(merc.isGeocentric() == false);

        SRS wgs84("epsg:4326"); // geographic WGS84 (long/lat/hae)
        REQUIRE(wgs84.valid());
        REQUIRE(wgs84.isProjected() == false);
        REQUIRE(wgs84.isGeographic() == true);
        REQUIRE(wgs84.isGeocentric() == false);

        auto xform = merc.to(wgs84);
        REQUIRE(xform.valid());

        dvec3 out;
        REQUIRE(xform(dvec3(-20037508.34278925, 0, 0), out));
        REQUIRE(equiv(out, dvec3(-180, 0, 0)));

        REQUIRE(xform.inverse(dvec3(0, 90, 0), out));
    }

    SECTION("geoSRS")
    {
        SRS merc("epsg:3785"); // spherical mercator SRS
        REQUIRE(merc.valid());

        SRS geo = merc.geoSRS();
        REQUIRE(geo.valid());
        REQUIRE(geo.isGeographic());

        SRS utm("epsg:32632"); // UTM32/WGS84
        REQUIRE(utm.geoSRS().isGeographic());
    }

    SECTION("wgs84/ecef")
    {
        SRS wgs84("epsg:4326"); // geographic WGS84 (long/lat/hae)
        REQUIRE(wgs84.valid());
        REQUIRE(wgs84.isProjected() == false);
        REQUIRE(wgs84.isGeographic() == true);
        REQUIRE(wgs84.isGeocentric() == false);

        SRS ecef("epsg:4978"); // geocentric WGS84 (ECEF)
        REQUIRE(ecef.valid());
        REQUIRE(ecef.isProjected() == false);
        REQUIRE(ecef.isGeographic() == false);
        REQUIRE(ecef.isGeocentric() == true);

        dvec3 out;

        auto xform_wgs84_to_ecef = wgs84.to(ecef);
        REQUIRE(xform_wgs84_to_ecef.valid());

        REQUIRE(xform_wgs84_to_ecef(dvec3(0, 0, 0), out));
        REQUIRE(equiv(out, dvec3(6378137, 0, 0)));

        REQUIRE(xform_wgs84_to_ecef.inverse(out, out));
        REQUIRE(equiv(out, dvec3(0, 0, 0)));
    }

    SECTION("plate-carree")
    {
        auto pc = SRS("plate-carree");
        REQUIRE(pc == SRS::PLATE_CARREE);
        REQUIRE(pc.isProjected() == true);
        REQUIRE(pc.isGeographic() == false);
        REQUIRE(pc.isGeocentric() == false);
        auto b = pc.bounds();
        REQUIRE((b.valid() &&
            equiv(b.xmin, -20037508.342, E) && equiv(b.xmax, 20037508.342, E) &&
            equiv(b.ymin, -10018754.171, E) && equiv(b.ymax, 10018754.171, E)));
    }

    SECTION("utm")
    {
        SRS utm32N("epsg:32632"); // +proj=utm +zone=32 +datum=WGS84
        REQUIRE(utm32N.valid());
        REQUIRE(utm32N.isProjected() == true);
        REQUIRE(utm32N.isGeographic() == false);
        REQUIRE(utm32N.isGeocentric() == false);
        REQUIRE(utm32N.bounds().valid());

        SRS utm32S("+proj=utm +zone=32 +south +datum=WGS84");
        REQUIRE(utm32S.valid());
        REQUIRE(utm32S.isProjected() == true);
        REQUIRE(utm32S.isGeographic() == false);
        REQUIRE(utm32S.isGeocentric() == false);
        auto b = utm32S.bounds();
        REQUIRE((b.valid() && b.xmin == 166000 && b.xmax == 834000 && b.ymin == 1116915 && b.ymax == 10000000));
    }

    SECTION("invalid")
    {
        SRS bad("gibberish");
        REQUIRE(bad.valid() == false);
        REQUIRE(bad.isProjected() == false);
        REQUIRE(bad.isGeographic() == false);
        REQUIRE(bad.isGeocentric() == false);
    }

    SECTION("vertical datum")
    {
        SRS wgs84("epsg:4326"); // geographic WGS84
        REQUIRE(wgs84.valid());

        SRS egm96("epsg:4326", "us_nga_egm96_15"); // WGS84 with EGM96 vdatum
        REQUIRE(egm96.valid());

        // Reference: http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm96/intpt.html
        dvec3 out;

        // geodetic to vdatum:
        {
            auto xform = wgs84.to(egm96);
            REQUIRE(xform.valid());

            REQUIRE(xform(dvec3(0, 0, 17.16), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(90, 0, -63.24), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(180, 0, 21.15), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform(dvec3(-90, 0, -4.29), out));
            REQUIRE(equiv(out.z, 0.0, E));

            // inverse
            REQUIRE(xform.inverse(dvec3(0, 0, 0), out));
            REQUIRE(equiv(out.z, 17.16, E));
            REQUIRE(xform.inverse(dvec3(90, 0, 0), out));
            REQUIRE(equiv(out.z, -63.24, E));
            REQUIRE(xform.inverse(dvec3(180, 0, 0), out));
            REQUIRE(equiv(out.z, 21.15, E));
            REQUIRE(xform.inverse(dvec3(-90, 0, 0), out));
            REQUIRE(equiv(out.z, -4.29, E));
        }

        // vdatum to geodetic:
        {
            auto xform = egm96.to(wgs84);
            REQUIRE(xform.valid());

            REQUIRE(xform(dvec3(0, 0, 0), out));
            REQUIRE(equiv(out.z, 17.16, E));
            REQUIRE(xform(dvec3(90, 0, 0), out));
            REQUIRE(equiv(out.z, -63.24, E));
            REQUIRE(xform(dvec3(180, 0, 0), out));
            REQUIRE(equiv(out.z, 21.15, E));
            REQUIRE(xform(dvec3(-90, 0, 0), out));
            REQUIRE(equiv(out.z, -4.29, E));

            // inverse
            REQUIRE(xform.inverse(dvec3(0, 0, 17.16), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(90, 0, -63.24), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(180, 0, 21.15), out));
            REQUIRE(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(dvec3(-90, 0, -4.29), out));
            REQUIRE(equiv(out.z, 0.0, E));
        }

        // vdatum to vdatum (noop)
        {
            auto xform = egm96.to(egm96);
            REQUIRE(xform.valid());

            dvec3 out;
            REQUIRE(xform(dvec3(0, 0, 17.16), out));
            REQUIRE(equiv(out.z, 17.16, E));
        }
    }

    SECTION("metadata")
    {
        Box b;
        b = SRS::WGS84.bounds();
        REQUIRE(equiv(b.xmin, -180.0));
        REQUIRE(equiv(b.xmax, 180.0));
        REQUIRE(equiv(b.ymin, -90.0));
        REQUIRE(equiv(b.ymax, 90.0));

        b = SRS::SPHERICAL_MERCATOR.bounds();
        REQUIRE(equiv(b.xmin, -20037508.342789244, E));
        REQUIRE(equiv(b.xmax, 20037508.342789244, E));
        REQUIRE(equiv(b.ymin, -20048966.104014594, E));
        REQUIRE(equiv(b.ymax, 20048966.104014594, E));

        auto ellipsoid = SRS::WGS84.ellipsoid();
        REQUIRE(ellipsoid.semiMajorAxis() == 6378137.0);

        Units units;
        units = SRS::WGS84.units();
        REQUIRE(units == Units::DEGREES);
        units = SRS::SPHERICAL_MERCATOR.units();
        REQUIRE(units == Units::METERS);
    }

    SECTION("multithreading")
    {
        // tests the fact that SRS are thread-specific
        auto function = []()
        {
            SRS a("wgs84");
            SRS b("spherical-mercator");
            auto xform = a.to(b);
            dvec3 out;
            REQUIRE(xform(dvec3(-180, 0, 0), out));
            REQUIRE(equiv(out, dvec3(-20037508.34278925, 0, 0)));
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

    SECTION("profiles")
    {
        Profile GG("global-geodetic");
        REQUIRE(GG.valid());
        REQUIRE(GG.srs() == SRS::WGS84);

        Profile SM("spherical-mercator");
        REQUIRE(SM.valid());
        REQUIRE(SM.srs() == SRS::SPHERICAL_MERCATOR);
    }
}
