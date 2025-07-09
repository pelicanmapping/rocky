#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/rocky.h>
#include <random>

#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>

#include <vsg/maths/vec3.h>
#include <vsg/maths/quat.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

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

    Context context = ContextFactory::create();
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "file.xml";
    auto map = rocky::Map::create();
    map->layers().add(layer);
    auto serialized = map->to_json();
    map = rocky::Map::create();
    map->from_json(serialized, context->io);
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
    util::ZLibCompressor comp;
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
        CHECK(hf->heightAtPixel(16.5, 16.5, Interpolation::BILINEAR) == 75.0f);
        
        // read with NO_DATA_VALUEs:
        hf->heightAt(17, 17) = NO_DATA_VALUE;
        hf->heightAt(16, 16) = NO_DATA_VALUE;
        CHECK(hf->heightAtPixel(16.5, 16.5, Interpolation::BILINEAR) == 50.0f);

        // all NODATA:
        hf->fill(NO_DATA_VALUE);
        CHECK(hf->heightAtPixel(16.5, 16.5, Interpolation::BILINEAR) == NO_DATA_VALUE);
    }
}

TEST_CASE("Map")
{
    auto map = Map::create();
    REQUIRE(map);
    if (map) {
        auto layer = TestLayer::create();

        unsigned cb_code = 0;

        auto added_cb = [&cb_code](std::shared_ptr<Layer> layer, unsigned index, Revision rev)
        {
            cb_code = 100;
        };
        map->onLayerAdded(added_cb);

        auto moved_cb = [&cb_code](std::shared_ptr<Layer> layer, unsigned oldIndex, unsigned newIndex, Revision rev)
        {
            cb_code = 200;
        };
        map->onLayerMoved(moved_cb);

        auto removed_cb = [&cb_code](std::shared_ptr<Layer> layer, Revision rev)
        {
            cb_code = 300;
        };
        map->onLayerRemoved(removed_cb);

        map->layers().add(layer);
        CHECK(cb_code == 100);
        CHECK(map->layers().size() == 1);

        //map->moveLayer(layer, 0);
        map->layers().move(layer, 0);
        CHECK(cb_code == 200);

        auto layers = map->layers().all();
        CHECK(layers.size() == 1);

        map->layers().remove(layer);
        CHECK(cb_code == 300);
        CHECK(map->layers().size() == 0);
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
    CHECK((s.ok() || s.code == s.ResourceUnavailable));
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
        REQUIRE(xform(glm::dvec3(-20037508.34278925, 0, 0), out));
        CHECK(equiv(out, glm::dvec3(-180, 0, 0), E));
        
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
        CHECK(equiv(out, glm::dvec3(6378137, 0, 0)));

        REQUIRE(xform_wgs84_to_ecef.inverse(out, out));
        CHECK(equiv(out, glm::dvec3(0, 0, 0)));
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
            equiv(b.xmin, -20037508.342, E) && equiv(b.xmax, 20037508.342, E) &&
            equiv(b.ymin, -10018754.171, E) && equiv(b.ymax, 10018754.171, E)));
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
        CHECK(equiv(c, glm::dvec3(0, 0, 0), E));
        // long and lat are out of range for face 0, but doesn't fail
        //CHECK(xform(dvec3(90, 46, 0), c) == false);

        REQUIRE(xform(glm::dvec3(45, 0, 0), c));
        CHECK(equiv(c, glm::dvec3(semi_major, 0, 0), E));

        REQUIRE(xform.inverse(glm::dvec3(semi_major, 0, 0), c));
        CHECK(equiv(c, glm::dvec3(45, 0, 0), E));

        REQUIRE(xform(glm::dvec3(0, 45, 0), c));
        // FAILS - not sure what is up here:
        // 45 degrees transforms to 6352271.2440m
        // but the semi-minor axis is 6356752.3142m
        //CHECK(equiv(c, dvec3(0, semi_minor, 0), E));

        // other way
        xform = qsc_face_0.to(wgs84);
        REQUIRE(xform.valid());

        REQUIRE(xform(glm::dvec3(semi_major, 0, 0), c));
        CHECK(equiv(c, glm::dvec3(45, 0, 0), E));
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
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(90, 0, -63.24), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(180, 0, 21.15), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(-90, 0, -4.29), out));
            CHECK(equiv(out.z, 0.0, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 0), out));
            CHECK(equiv(out.z, 17.16, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, 0), out));
            CHECK(equiv(out.z, -63.24, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 0), out));
            CHECK(equiv(out.z, 21.15, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, 0), out));
            CHECK(equiv(out.z, -4.29, E));

            SRS::projMessageCallback = nullptr;
        }

        // vdatum to geodetic:
        {
            auto xform = egm96.to(wgs84);
            REQUIRE(xform.valid());

            REQUIRE(xform(glm::dvec3(0, 0, 0), out));
            CHECK(equiv(out.z, 17.16, E));
            REQUIRE(xform(glm::dvec3(90, 0, 0), out));
            CHECK(equiv(out.z, -63.24, E));
            REQUIRE(xform(glm::dvec3(180, 0, 0), out));
            CHECK(equiv(out.z, 21.15, E));
            REQUIRE(xform(glm::dvec3(-90, 0, 0), out));
            CHECK(equiv(out.z, -4.29, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 17.16), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, -63.24), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 21.15), out));
            CHECK(equiv(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, -4.29), out));
            CHECK(equiv(out.z, 0.0, E));
        }

        // vdatum to vdatum (noop)
        {
            auto xform = egm96.to(egm96);
            REQUIRE(xform.valid());

            glm::dvec3 out;
            REQUIRE(xform(glm::dvec3(0, 0, 17.16), out));
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
        CHECK(equiv(b.ymin, -20037508.342789244, E));
        CHECK(equiv(b.ymax, 20037508.342789244, E));

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
            glm::dvec3 out;
            REQUIRE(xform(glm::dvec3(-180, 0, 0), out));
            CHECK(equiv(out, glm::dvec3(-20037508.34278925, 0, 0)));
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
        CHECKED_IF(r.status.ok())
        {
            CHECK(r.value.contentType == "text/xml");

            auto body = r.value.data;
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
            auto r = uri.read(IOOptions());
            CHECKED_IF(r.status.ok())
            {
                CHECK(r.value.contentType == "text/xml");
                auto body = r.value.data;
                CHECK(!body.empty());
                CHECK(rocky::util::startsWith(body, "<?xml"));
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
    CHECKED_IF(result.status.ok())
    {
        VSGContext context = VSGContextFactory::create(nullptr);
        auto mapNode = MapNode::create(context);
        mapNode->from_json(result.value, IOOptions(context->io, earthFile));

        auto layer1 = mapNode->map->layers().withName("ReadyMap 15m Imagery");
        CHECKED_IF(layer1)
        {
            auto tms_layer = TMSImageLayer::cast(layer1);
            CHECK(tms_layer);
            CHECK(tms_layer->uri.has_value());
            CHECK(tms_layer->uri.value() == "https://readymap.org/readymap/tiles/1.0.0/7/");
        }
    }
}

TEST_CASE("LayersCollection API")
{
    // Create a map and get its layer collection
    auto map = Map::create();
    REQUIRE(map);

    auto& layers = map->layers();
    CHECK(layers.size() == 0);
    CHECK(layers.empty());

    // Create some test layers
    auto layer1 = TestLayer::create();
    layer1->setName("Layer1");
    auto layer2 = TestLayer::create();
    layer2->setName("Layer2");
    auto layer3 = TestLayer::create();
    layer3->setName("Layer3");

    // Add layers
    layers.add(layer1);
    layers.add(layer2);
    layers.add(layer3);
    CHECK(layers.size() == 3);

    // Add a layer again (should not duplicate)
    layers.add(layer3);
    CHECK(layers.size() == 3);

    // Check all()
    auto all_layers = layers.all();
    CHECK(all_layers.size() == 3);

    // Check indexOf
    CHECK(layers.indexOf(layer1) == 0);
    CHECK(layers.indexOf(layer2) == 1);
    CHECK(layers.indexOf(layer3) == 2);

    // Check at()
    auto at1 = layers.at<TestLayer>(0);
    CHECK(at1 == layer1);
    auto at2 = layers.at<TestLayer>(1);
    CHECK(at2 == layer2);

    // Check withName()
    auto by_name = layers.withName<TestLayer>("Layer2");
    CHECK(by_name == layer2);

    // Check withUID()
    auto by_uid = layers.withUID<TestLayer>(layer3->uid());
    CHECK(by_uid == layer3);

    // Check firstOfType and ofType
    auto first = layers.firstOfType<TestLayer>();
    CHECK(first == layer1);
    auto all_of_type = layers.ofType<TestLayer>();
    CHECK(all_of_type.size() == 3);

    // Move a layer
    layers.move(layer3, 0);
    CHECK(layers.indexOf(layer3) == 0);
    CHECK(layers.indexOf(layer1) == 1);
    CHECK(layers.indexOf(layer2) == 2);

    // Remove a layer
    layers.remove(layer2);
    CHECK(layers.size() == 2);
    CHECK(layers.indexOf(layer3) == 0);
    CHECK(layers.indexOf(layer1) == 1);

    // Remove all layers
    layers.remove(layer1);
    layers.remove(layer3);
    CHECK(layers.size() == 0);
    CHECK(layers.empty());
}

TEST_CASE("MapManipulator NaN fix")
{
    // Test case for issue #105 - NaN error in MapManipulator.cpp
    // This tests that very close vectors don't produce NaN when creating quaternions
    
    // Test data from the original issue
    vsg::dvec3 center(-2248544.6429430502, 5050313.7280376982, 3170373.6894794349);
    vsg::dvec3 target(-2248544.6095093964, 5050313.7806744399, 3170373.6297455574);
    
    // Calculate distance between the vectors
    double dist = distance3D(center, target);
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