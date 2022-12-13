#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/Instance.h>
#include <rocky/Map.h>
#include <rocky/Math.h>
#include <rocky/Notify.h>
#include <rocky/Image.h>
#include <rocky/Heightfield.h>
#include <rocky/GDALLayers.h>

using namespace rocky;

namespace
{
    class TestLayer : public Inherit<Layer, TestLayer>
    {
    public:
        Status openImplementation(const IOOptions& io) override {
            return STATUS_OK;
        }
    };

    bool eq(const dvec3& a, const dvec3& b) {
        return equivalent(a.x, b.x) && equivalent(a.y, b.y) && equivalent(a.z, b.z);
    }
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
    std::cout << r.x << ", " << r.y << ", " << r.z << std::endl;
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
    image->fill(Color::Orange); // fvec4(1, 0.5, 0.0, 1));
    Image::Pixel value;
    image->read(value, 17, 17);
    std::cout << value.r << ", " << value.g << ", " << value.b << ", " << value.a << std::endl;
    REQUIRE(equivalent(value.r, 1.0f, 0.01f));
    REQUIRE(equivalent(value.g, 0.5f, 0.01f));
    REQUIRE(equivalent(value.b, 0.0f, 0.01f));
    REQUIRE(equivalent(value.a, 1.0f, 0.01f));
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
        REQUIRE(map->getNumLayers() == 1);

        map->removeLayer(layer);
        REQUIRE(map->getNumLayers() == 0);
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
    SECTION("Caching")
    {
        auto srs1 = SRS::get("spherical-mercator");
        auto srs2 = SRS::get("spherical-mercator");
        REQUIRE(srs1.get() == srs2.get());
    }

    SECTION("smerc")
    {
        // Create a spherical mercator SRS.
        auto mercSRS = SRS::get("spherical-mercator");
        REQUIRE(mercSRS);
        if (mercSRS) {
            REQUIRE(mercSRS->isSphericalMercator());
            REQUIRE(mercSRS->isProjected());
            REQUIRE(!mercSRS->isGeodetic());
            REQUIRE(!mercSRS->isGeographic());

            auto epsg900913 = SRS::get("epsg:900913");
            REQUIRE((epsg900913 && epsg900913->isEquivalentTo(mercSRS)));

            auto epsg3785 = SRS::get("epsg:3785");
            REQUIRE((epsg3785 && epsg3785->isEquivalentTo(mercSRS)));

            auto epsg102113 = SRS::get("epsg:102113");
            REQUIRE((epsg102113 && epsg102113->isEquivalentTo(mercSRS)));
        }
    }

    SECTION("wgs84")
    {
        // Create a wgs84 mercator SRS.
        auto wgs84 = SRS::get("wgs84");
        REQUIRE(wgs84);
        if (wgs84) {
            REQUIRE(wgs84->isGeographic());
            REQUIRE(wgs84->isGeodetic());
            REQUIRE(!wgs84->isMercator());
            REQUIRE(!wgs84->isProjected());

            auto epsg4326 = SRS::get("epsg:4326");
            REQUIRE((epsg4326 && epsg4326->isEquivalentTo(wgs84)));

            auto sm = SRS::get("spherical-mercator");
            REQUIRE((sm && wgs84 && !wgs84->isEquivalentTo(sm)));
        }
    }

    SECTION("plate carre")
    {
        auto pc = SRS::get("plate-carree");
        REQUIRE(pc);
        auto wgs84 = SRS::get("wgs84");
        REQUIRE(wgs84);
        if (pc && wgs84) {
            REQUIRE(!pc->isGeographic());
            REQUIRE(!pc->isMercator());
            REQUIRE(!pc->isGeodetic());
            REQUIRE(pc->isProjected());

            double pcMinX = -20037508.3427892476320267;
            double pcMaxX = 20037508.3427892476320267;
            double pcMinY = -10018754.1713946219533682;
            double pcMaxY = 10018754.1713946219533682;

            dvec3 output;
            REQUIRE(wgs84->transform(dvec3(-180, -90, 0), pc, output));
            REQUIRE(eq(output, dvec3(pcMinX, pcMinY, 0)));

            REQUIRE(wgs84->transform(dvec3(-180, +90, 0), pc, output));
            REQUIRE(eq(output, dvec3(pcMinX, pcMaxY, 0)));

            REQUIRE(wgs84->transform(dvec3(180, -90, 0), pc, output));
            REQUIRE(eq(output, dvec3(pcMaxX, pcMinY, 0)));

            REQUIRE(wgs84->transform(dvec3(180, 90, 0), pc, output));
            REQUIRE(eq(output, dvec3(pcMaxX, pcMaxY, 0)));
        }
    }

    SECTION("ellipsoids")
    {
        auto merc = SRS::get("spherical-mercator", "egm96");
        REQUIRE(merc);
        if (merc) {
            // merc to geographic
            auto geog = merc->getGeographicSRS();
            REQUIRE(geog);
            if (geog) {
                REQUIRE(geog->isGeographic());
                REQUIRE(geog->getVerticalDatum() != nullptr);
                REQUIRE(geog->isGeodetic() == false);
            }

            // geodetic
            auto geod = merc->getGeodeticSRS();
            REQUIRE(geod);
            if (geod) {
                REQUIRE(geod->isGeographic());
                REQUIRE(geod->isGeodetic());
                REQUIRE(geod->getVerticalDatum() == nullptr);
            }

            // geodetic to geocentric
            if (geod) {
                auto geoc = geod->getGeocentricSRS();
                REQUIRE(geoc);
                if (geoc) {
                    dvec3 np_geod(0.0, 90.0, 0.0);
                    dvec3 np_ecef(0.0, 0.0, geod->getEllipsoid().getRadiusPolar());
                    dvec3 temp;
                    REQUIRE(geod->transform(np_geod, geoc, temp));
                    REQUIRE(eq(temp, np_ecef));
                    REQUIRE(geoc->transform(np_ecef, geod, temp));
                    REQUIRE(eq(temp, np_geod));
                }
            }
        }
    }

    SECTION("egm96")
    {
        auto wgs84 = SRS::get("wgs84");
        
        auto wgs84_egm96 = SRS::get("wgs84", "egm96");
        REQUIRE(wgs84_egm96);
        REQUIRE((wgs84_egm96 && wgs84_egm96->getVerticalDatum()));

        double eps = 0.2;

        // Vertical datum tests.
        // Reference: https://earth-info.nga.mil/index.php?dir=wgs84&action=egm96-geoid-calc
        dvec3 output;

        REQUIRE(wgs84->transform(dvec3(0, 0, 17.16), wgs84_egm96, output));
        REQUIRE(equivalent(output.z, 0.0, eps));

        REQUIRE(wgs84->transform(dvec3(180, 0, 21.15), wgs84_egm96, output));
        REQUIRE(equivalent(output.z, 0.0, eps));

        REQUIRE(wgs84->transform(dvec3(-90, 0, -4.29), wgs84_egm96, output));
        REQUIRE(equivalent(output.z, 0.0, eps));

        REQUIRE(wgs84->transform(dvec3(90, 0, -63.24), wgs84_egm96, output));
        REQUIRE(equivalent(output.z, 0.0, eps));
    }

    SECTION("invalid")
    {
        auto invalid = SRS::get("an invalid srs");
        REQUIRE(invalid == nullptr);
    }
}
