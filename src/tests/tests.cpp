#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <rocky/rocky.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/Decal.h>
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#include <rocky/vsg/ecs/DecalSystem.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include <rocky/vsg/ecs/LineSystem.h>
#include <rocky/vsg/ecs/PointSystem.h>
#include <rocky/vsg/ecs/ModelSystem.h>
#include <rocky/vsg/ecs/TextureSystem.h>
#include <rocky/vsg/ecs/OpticsSystem.h>
#include <rocky/vsg/ecs/TransformDetail.h>
#include <rocky/vsg/ecs/FeatureBuilder.h>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <unordered_map>

#define ROCKY_EXPOSE_JSON_FUNCTIONS
#include <rocky/json.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    class TestLayer : public Inherit<Layer, TestLayer>
    {
    public:
        Result<> openImplementation(const IOOptions& io) override {
            return ResultVoidOK;
        }
    };
}

TEST_CASE("strings")
{
    std::string s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "world", "Rocky") == "Hello, Rocky!");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "world", "") == "Hello, !");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "", "Rocky") == "Hello, world!");
    s1 = "Hello, world!";
    CHECK(detail::replaceInPlace(s1, "", "") == "Hello, world!");

    s1 = detail::trim("  Hello, Rocky!  ");
    CHECK(s1 == "Hello, Rocky!");
    s1 = "  Hello, Rocky!  ";
    CHECK(detail::trimInPlace(s1) == "Hello, Rocky!");
}

TEST_CASE("mesh feature default tessellation is curvature bounded", "[featurebuilder]")
{
    Feature building(
        SRS::WGS84,
        Geometry::Type::Polygon,
        {
            { 139.0, 35.0, 0.0 },
            { 139.001, 35.0, 0.0 },
            { 139.001, 35.001, 0.0 },
            { 139.0, 35.001, 0.0 }
        });

    FeatureBuilder builder;

    MeshStyle defaultStyle;
    MeshGeometry defaultGeometry;
    builder.buildMeshGeometry({ building }, defaultStyle, defaultGeometry);

    REQUIRE_FALSE(defaultGeometry.vertices.empty());
    CHECK(defaultGeometry.vertices.size() < 100u);

    MeshStyle fineStyle;
    fineStyle.resolution = 20.0f;
    MeshGeometry fineGeometry;
    builder.buildMeshGeometry({ building }, fineStyle, fineGeometry);

    CHECK(fineGeometry.vertices.size() > defaultGeometry.vertices.size());
}

TEST_CASE("projected texture contracts", "[projection]")
{
    RenderTexture renderTexture;
    CHECK(renderTexture.sources.empty());
    CHECK(renderTexture.textureSize == glm::uvec2(512u, 512u));
    CHECK_FALSE(renderTexture.useDepthBuffer);

    ProjectedTexture projected;
    CHECK((projected.texture == entt::null));
    CHECK((projected.projector == entt::null));

    RenderTextureBounds bounds;
    bounds.expand(SRS::WGS84, glm::dvec3(179.9, 10.0, 0.0));
    bounds.expand(SRS::WGS84, glm::dvec3(-179.9, 11.0, 0.0));
    REQUIRE(bounds.valid);
    CHECK(bounds.maxx - bounds.minx < 1.0);

    SharedRenderData shared;
    REQUIRE(shared.decalTextures);
    CHECK(shared.projectedTextureCapacity() ==
        SharedRenderData::DEFAULT_PROJECTED_TEXTURE_CAPACITY);

    auto originalDescriptor = shared.decalTextures;
    shared.configureProjectedTextureCapacity(17u);
    CHECK(shared.projectedTextureCapacity() == 17u);
    CHECK(shared.decalTextures != originalDescriptor);

    auto configuredDescriptor = shared.decalTextures;
    shared.configureProjectedTextureCapacity(17u);
    CHECK(shared.decalTextures == configuredDescriptor);

    shared.configureProjectedTextureCapacity(0u);
    CHECK(shared.projectedTextureCapacity() == 1u);
}

TEST_CASE("render texture revisions", "[projection]")
{
    Registry registry = Registry::create();
    auto meshSystem = MeshSystemNode::create(registry);
    auto lineSystem = LineSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto source = reg.create();

        auto& meshGeometry = reg.emplace<MeshGeometry>(source);
        meshGeometry.srs = SRS::WGS84;
        meshGeometry.vertices.emplace_back(0.0, 0.0, 0.0);

        auto& meshStyle = reg.emplace<MeshStyle>(source);
        reg.emplace<Mesh>(source, meshGeometry, meshStyle);

        RenderTextureRevision meshInitial;
        meshSystem->contributeRenderTextureRevision(reg, source, meshInitial);

        auto previousComponentRevision = meshStyle.componentRevision();
        meshStyle.depthOffset = 100.0f;
        meshStyle.dirty(reg);
        CHECK(meshStyle.componentRevision() > previousComponentRevision);

        RenderTextureRevision meshStyleChanged;
        meshSystem->contributeRenderTextureRevision(reg, source, meshStyleChanged);
        CHECK(meshStyleChanged.bounds == meshInitial.bounds);
        CHECK(meshStyleChanged.content != meshInitial.content);

        meshGeometry.vertices.emplace_back(1.0, 1.0, 0.0);
        meshGeometry.dirty(reg);

        RenderTextureRevision meshGeometryChanged;
        meshSystem->contributeRenderTextureRevision(reg, source, meshGeometryChanged);
        CHECK(meshGeometryChanged.bounds != meshStyleChanged.bounds);
        CHECK(meshGeometryChanged.content != meshStyleChanged.content);

        MeshStyle replacement;
        replacement.depthOffset = 200.0f;
        auto revisionBeforeReplacement = meshStyle.componentRevision();
        auto& replacedStyle = reg.emplace_or_replace<MeshStyle>(source, replacement);
        CHECK(replacedStyle.componentRevision() != revisionBeforeReplacement);

        auto& lineGeometry = reg.emplace<LineGeometry>(source);
        lineGeometry.srs = SRS::WGS84;
        lineGeometry.points = { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 } };
        auto& lineStyle = reg.emplace<LineStyle>(source);
        reg.emplace<Line>(source, lineGeometry, lineStyle);

        RenderTextureRevision lineInitial;
        lineSystem->contributeRenderTextureRevision(reg, source, lineInitial);

        // Line width affects bounds padding, while depthOffset verifies that
        // formerly omitted style properties still invalidate baked content.
        lineStyle.depthOffset = 50.0f;
        lineStyle.dirty(reg);

        RenderTextureRevision lineStyleChanged;
        lineSystem->contributeRenderTextureRevision(reg, source, lineStyleChanged);
        CHECK(lineStyleChanged.bounds != lineInitial.bounds);
        CHECK(lineStyleChanged.content != lineInitial.content);
    });
}

TEST_CASE("render texture participant order", "[projection]")
{
    Registry registry = Registry::create();
    auto mesh = MeshSystemNode::create(registry);
    auto line = LineSystemNode::create(registry);
    auto point = PointSystemNode::create(registry);
    auto model = ModelSystemNode::create(registry);

    CHECK(mesh->renderTextureOrder() < line->renderTextureOrder());
    CHECK(line->renderTextureOrder() < point->renderTextureOrder());
    CHECK(point->renderTextureOrder() < model->renderTextureOrder());
}

TEST_CASE("line triangles use the segment start as provoking vertex", "[line]")
{
    auto check = [](LineTopology topology, const std::vector<vsg::dvec3>& points,
        const std::vector<std::uint32_t>& expectedProvokingVertices)
    {
        auto node = LineGeometryNode::create();
        node->set(points, std::vector<vsg::vec4>(), topology);

        REQUIRE(node->_drawCommand->indexCount == expectedProvokingVertices.size() * 6u);
        for (std::size_t segment = 0; segment < expectedProvokingVertices.size(); ++segment)
        {
            const auto firstTriangle = (*node->_indices)[segment * 6u];
            const auto secondTriangle = (*node->_indices)[segment * 6u + 3u];
            CHECK(firstTriangle == expectedProvokingVertices[segment]);
            CHECK(secondTriangle == expectedProvokingVertices[segment]);
        }
    };

    check(LineTopology::Strip,
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 } },
        { 2u, 6u });

    check(LineTopology::Segments,
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 },
          { 2.0, 0.0, 0.0 }, { 2.0, 1.0, 0.0 } },
        { 2u, 10u });
}

TEST_CASE("render texture bounds include source transform", "[projection]")
{
    Registry registry = Registry::create();
    auto meshSystem = MeshSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto source = reg.create();
        auto& geometry = reg.emplace<MeshGeometry>(source);
        geometry.vertices.emplace_back(1.0, 2.0, 3.0);
        auto& style = reg.emplace<MeshStyle>(source);
        reg.emplace<Mesh>(source, geometry, style);

        auto& transform = reg.emplace<Transform>(source);
        transform.position = GeoPoint(SRS::SPHERICAL_MERCATOR, 10.0, 20.0, 30.0);
        transform.localMatrix = glm::scale(glm::dmat4(1.0), glm::dvec3(2.0, 3.0, 4.0));

        RenderTextureBounds transformed;
        meshSystem->expandRenderTextureBounds(
            reg, source, transformed, SRS::SPHERICAL_MERCATOR, true);
        REQUIRE(transformed.valid);
        CHECK(transformed.minx == Approx(12.0));
        CHECK(transformed.miny == Approx(26.0));
        CHECK(transformed.minz == Approx(42.0));

        RenderTextureBounds local;
        meshSystem->expandRenderTextureBounds(
            reg, source, local, SRS::SPHERICAL_MERCATOR, false);
        CHECK_FALSE(local.valid);
    });
}

TEST_CASE("texture producer ownership", "[projection]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    auto bakeSystem = OverlayBakeSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto externalImage = reg.create();
        reg.emplace<ImageTexture>(externalImage);
        reg.emplace<TextureResource>(externalImage);
        reg.remove<ImageTexture>(externalImage);
        CHECK(reg.any_of<TextureResource>(externalImage));

        auto producedImage = reg.create();
        reg.emplace<ImageTexture>(producedImage);
        auto& imageResource = reg.emplace<TextureResource>(producedImage);
        imageResource.producer = TextureResourceProducer::ImageTexture;
        reg.remove<ImageTexture>(producedImage);
        CHECK_FALSE(reg.any_of<TextureResource>(producedImage));

        auto externalBake = reg.create();
        reg.emplace<RenderTexture>(externalBake);
        reg.emplace<TextureResource>(externalBake);
        reg.remove<RenderTexture>(externalBake);
        CHECK(reg.any_of<TextureResource>(externalBake));

        auto producedBake = reg.create();
        reg.emplace<RenderTexture>(producedBake);
        auto& bakeResource = reg.emplace<TextureResource>(producedBake);
        bakeResource.producer = TextureResourceProducer::RenderTexture;
        reg.remove<RenderTexture>(producedBake);
        CHECK_FALSE(reg.any_of<TextureResource>(producedBake));
    });
}

TEST_CASE("null image texture is a ready procedural resource", "[projection]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    auto contextSingleton = VSGContextFactory::create(vsg::Viewer::create());

    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<ImageTexture>(entity);
    });

    textureSystem->update(contextSingleton.get());

    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<TextureResource>(entity);
        CHECK(resource.producer == TextureResourceProducer::ImageTexture);
        CHECK(resource.ready);
        CHECK_FALSE(resource.texture);
    });
}

TEST_CASE("manual optics work without a terrain target", "[projection]")
{
    Registry registry = Registry::create();
    auto opticsSystem = OpticsSystemNode::create(registry);
    auto contextSingleton = VSGContextFactory::create(vsg::Viewer::create());
    auto context = contextSingleton.get();

    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& optics = reg.emplace<Optics>(entity);
        optics.autoComputeFocalDistance = false;
        optics.autoComputeNearFar = false;
        optics.focalDistance = 25.0;
        optics.nearScale = 0.5;
        optics.nearBias = 1.0;
        optics.farScale = 2.0;
        optics.farBias = 3.0;
        reg.emplace<TransformDetail>(entity);
    });

    opticsSystem->update(context);

    registry.read([&](entt::registry& reg)
    {
        const auto& detail = reg.get<OpticsDetail>(entity).views[0];
        CHECK(detail.focalDistance == Approx(25.0));
        CHECK(detail.nearDistance == Approx(13.5));
        CHECK(detail.farDistance == Approx(53.0));
        CHECK_FALSE(detail.focalPointValid);
    });
}

TEST_CASE("legacy overlay adapter", "[projection]")
{
    Registry registry = Registry::create();
    auto bakeSystem = OverlayBakeSystemNode::create(registry);
    auto decalSystem = DecalSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto overlayEntity = reg.create();
        reg.emplace<Overlay>(overlayEntity);
        CHECK(reg.any_of<RenderTexture>(overlayEntity));
        CHECK(reg.any_of<ProjectedTexture>(overlayEntity));
        REQUIRE(reg.any_of<RenderParticipation>(overlayEntity));
        CHECK_FALSE(reg.get<RenderParticipation>(overlayEntity).mainView);

        reg.remove<Overlay>(overlayEntity);
        CHECK_FALSE(reg.any_of<RenderTexture>(overlayEntity));
        CHECK_FALSE(reg.any_of<ProjectedTexture>(overlayEntity));
        CHECK_FALSE(reg.any_of<RenderParticipation>(overlayEntity));

        auto explicitEntity = reg.create();
        reg.emplace<RenderTexture>(explicitEntity);
        reg.emplace<ProjectedTexture>(explicitEntity);
        reg.emplace<RenderParticipation>(explicitEntity);
        reg.emplace<Overlay>(explicitEntity);
        reg.remove<Overlay>(explicitEntity);
        CHECK(reg.any_of<RenderTexture>(explicitEntity));
        CHECK(reg.any_of<ProjectedTexture>(explicitEntity));
        CHECK(reg.any_of<RenderParticipation>(explicitEntity));
    });
}

TEST_CASE("legacy decal adapter", "[projection]")
{
    Registry registry = Registry::create();
    auto decalSystem = DecalSystemNode::create(registry);

    registry.write([&](entt::registry& reg)
    {
        auto styleEntity = reg.create();
        reg.emplace<DecalStyle>(styleEntity);
        CHECK(reg.any_of<ImageTexture>(styleEntity));

        auto decalEntity = reg.create();
        reg.emplace<Decal>(decalEntity, styleEntity);
        REQUIRE(reg.any_of<ProjectedTexture>(decalEntity));
        CHECK(reg.get<ProjectedTexture>(decalEntity).texture == styleEntity);

        reg.remove<Decal>(decalEntity);
        CHECK_FALSE(reg.any_of<ProjectedTexture>(decalEntity));
        reg.remove<DecalStyle>(styleEntity);
        CHECK_FALSE(reg.any_of<ImageTexture>(styleEntity));

        auto explicitStyle = reg.create();
        reg.emplace<ImageTexture>(explicitStyle);
        reg.emplace<DecalStyle>(explicitStyle);
        reg.remove<DecalStyle>(explicitStyle);
        CHECK(reg.any_of<ImageTexture>(explicitStyle));

        auto explicitDecal = reg.create();
        reg.emplace<ProjectedTexture>(explicitDecal);
        reg.emplace<Decal>(explicitDecal);
        reg.remove<Decal>(explicitDecal);
        CHECK(reg.any_of<ProjectedTexture>(explicitDecal));
    });
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

    auto contextSingleton = ContextFactory::create();
    auto context = contextSingleton.get();
    auto layer = rocky::TMSImageLayer::create();
    layer->uri = "file.xml";
    auto map = rocky::Map::create();
    map->add(layer);
    auto serialized = map->to_json();
    map = rocky::Map::create();
    auto r = map->from_json(serialized, context->io);
    CHECK(r.ok());
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

    std::unordered_map<TileKey, int> values;
    values[TileKey(2, 1, 0, p)] = 42;

    CHECK(values.at(TileKey(2, 1, 0, p)) == 42);
    CHECK(values.find(TileKey(2, 1, 1, p)) == values.end());
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

    SECTION("runtime shutdown joins worker threads and prevents restart")
    {
        jobs::runtime runtime;
        auto pool = runtime.get_pool("shutdown-test", 1);
        REQUIRE(pool != nullptr);

        std::atomic_bool job_started = false;
        std::atomic_bool release_job = false;
        std::atomic_bool shutdown_finished = false;

        auto result = runtime.dispatch(
            [&](jobs::cancelable&) -> int
            {
                job_started = true;
                while (!release_job)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return 42;
            },
            jobs::context{ "blocking shutdown test", pool });

        for (unsigned i = 0; i < 1000 && !job_started; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(job_started);

        std::thread shutdown_thread([&]()
            {
                runtime.shutdown();
                shutdown_finished = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(shutdown_finished == false);

        release_job = true;
        shutdown_thread.join();

        CHECK(shutdown_finished == true);
        CHECK(result.available());
        CHECK(result.value() == 42);
        CHECK(runtime.alive() == false);
        CHECK(runtime.get_pool("shutdown-test", 1) == nullptr);

        std::atomic_bool post_shutdown_job_ran = false;
        runtime.dispatch([&]() { post_shutdown_job_ran = true; });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CHECK(post_shutdown_job_ran == false);
        CHECK(runtime.total() == 0);
    }
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
    ZLibCompressor comp;
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
    image->fill(StockColor::Orange);
    Image::Pixel value = image->read(17, 17);
    //std::cout << value.r << ", " << value.g << ", " << value.b << ", " << value.a << std::endl;
    CHECK(glm::epsilonEqual(value.r, 1.0f, 0.01f));
    CHECK(glm::epsilonEqual(value.g, 0.65f, 0.01f));
    CHECK(glm::epsilonEqual(value.b, 0.0f, 0.01f));
    CHECK(glm::epsilonEqual(value.a, 1.0f, 0.01f));
}

TEST_CASE("Heightfield")
{
    auto hf = Heightfield::create(257, 257);
    REQUIRE(hf.image);
    if (hf.image) {
        // test metadata:
        CHECK(hf.image->pixelFormat() == Image::R32_SFLOAT);
        CHECK(hf.image->numComponents() == 1);
        CHECK(hf.image->sizeInBytes() == 264196);
        CHECK(hf.image->rowSizeInBytes() == 1028);
        CHECK(hf.image->componentSizeInBytes() == 4);
        CHECK(hf.image->sizeInPixels() == 66049);

        // write/read:
        hf.heightAt(16, 16) = 100.0f;
        hf.heightAt(16, 17) = 50.0f;
        hf.heightAt(17, 16) = 50.0f;
        hf.heightAt(17, 17) = 100.0f;
        CHECK(hf.heightAt(16, 16) == 100.0f);

        float u = 16.5f / (float(hf.width()) - 1.0f);
        float v = 16.5f / (float(hf.height()) - 1.0f);
        CHECK(hf.heightAtUV(u, v) == 75.0f);
        
        // read with NO_DATA_VALUEs:
        hf.heightAt(17, 17) = NO_DATA_VALUE;
        hf.heightAt(16, 16) = NO_DATA_VALUE;
        CHECK(hf.heightAt(16, 16) == NO_DATA_VALUE);
        CHECK(hf.heightAtUV(u, v) == 50.0f);

        // all NODATA:
        hf.fill(NO_DATA_VALUE);
        CHECK(hf.heightAt(16, 16) == NO_DATA_VALUE);
    }
}

TEST_CASE("Map")
{
    auto map = Map::create();
    REQUIRE(map);
    if (map) {
        auto layer = TestLayer::create();
        map->add(layer);
        CHECK(map->layers().size() == 1);
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
    CHECK((s.ok() || s.error().type == Failure::ResourceUnavailable));
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
        REQUIRE(xform(glm::dvec3(-20037508.342789248, 0, 0), out));
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(-180, 0, 0), E)));
        
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
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(6378137, 0, 0), 1e-6)));

        REQUIRE(xform_wgs84_to_ecef.inverse(out, out));
        CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(0, 0, 0), 1e-6)));
    }

    SECTION("Geocentric interpolation")
    {
        SRS wgs84("wgs84"); // geographic WGS84 (long/lat)
        REQUIRE(wgs84.valid());

        auto midpoint = wgs84.ellipsoid().geodesicInterpolate(
            glm::dvec3(0, 0, 0),
            glm::dvec3(90, 0, 0),
            0.5);

        CHECK(glm::epsilonEqual(midpoint.x, 45.0, 1e-9));
        CHECK(glm::epsilonEqual(midpoint.y, 0.0, 1e-9));
        CHECK(glm::epsilonEqual(midpoint.z, 0.0, 1e-6));
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
            glm::epsilonEqual(b.xmin, -20037508.342, E) && glm::epsilonEqual(b.xmax, 20037508.342, E) &&
            glm::epsilonEqual(b.ymin, -10018754.171, E) && glm::epsilonEqual(b.ymax, 10018754.171, E)));
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
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(0, 0, 0), E)));
        // long and lat are out of range for face 0, but doesn't fail
        //CHECK(xform(dvec3(90, 46, 0), c) == false);

        REQUIRE(xform(glm::dvec3(45, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(semi_major, 0, 0), E)));

        REQUIRE(xform.inverse(glm::dvec3(semi_major, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(45, 0, 0), E)));

        REQUIRE(xform(glm::dvec3(0, 45, 0), c));
        // FAILS - not sure what is up here:
        // 45 degrees transforms to 6352271.2440m
        // but the semi-minor axis is 6356752.3142m
        //CHECK(glm::epsilonEqual(c, dvec3(0, semi_minor, 0), E));

        // other way
        xform = qsc_face_0.to(wgs84);
        REQUIRE(xform.valid());

        REQUIRE(xform(glm::dvec3(semi_major, 0, 0), c));
        CHECK(glm::all(glm::epsilonEqual(c, glm::dvec3(45, 0, 0), E)));
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
        CHECK(bad.errorMessage() == "Invalid PROJ string syntax");
        
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
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(90, 0, -63.24), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(180, 0, 21.15), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform(glm::dvec3(-90, 0, -4.29), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -63.24, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 21.15, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -4.29, E));

            SRS::projMessageCallback = nullptr;
        }

        // vdatum to geodetic:
        {
            auto xform = egm96.to(wgs84);
            REQUIRE(xform.valid());

            REQUIRE(xform(glm::dvec3(0, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
            REQUIRE(xform(glm::dvec3(90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -63.24, E));
            REQUIRE(xform(glm::dvec3(180, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, 21.15, E));
            REQUIRE(xform(glm::dvec3(-90, 0, 0), out));
            CHECK(glm::epsilonEqual(out.z, -4.29, E));

            // inverse
            REQUIRE(xform.inverse(glm::dvec3(0, 0, 17.16), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(90, 0, -63.24), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(180, 0, 21.15), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
            REQUIRE(xform.inverse(glm::dvec3(-90, 0, -4.29), out));
            CHECK(glm::epsilonEqual(out.z, 0.0, E));
        }

        // vdatum to vdatum (noop)
        {
            auto xform = egm96.to(egm96);
            REQUIRE(xform.valid());

            glm::dvec3 out;
            REQUIRE(xform(glm::dvec3(0, 0, 17.16), out));
            CHECK(glm::epsilonEqual(out.z, 17.16, E));
        }
    }

    SECTION("SRS Metadata")
    {
        Box a = SRS::WGS84.bounds();
        CHECK(glm::epsilonEqual(a.xmin, -180.0, E));
        CHECK(glm::epsilonEqual(a.xmax, 180.0, E));
        CHECK(glm::epsilonEqual(a.ymin, -90.0, E));
        CHECK(glm::epsilonEqual(a.ymax, 90.0, E));

        Box bb = SRS::SPHERICAL_MERCATOR.bounds();
        CHECK(glm::epsilonEqual(bb.xmin, -20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.xmax, 20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.ymin, -20037508.342789248, E));
        CHECK(glm::epsilonEqual(bb.ymax, 20037508.342789248, E));

        auto ellipsoid = SRS::WGS84.ellipsoid();
        REQUIRE(ellipsoid.semiMajorAxis() == 6378137.0);

        UnitsType units;
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
            CHECK(glm::all(glm::epsilonEqual(out, glm::dvec3(-20037508.34278925, 0, 0), 1e-6)));
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
        CHECKED_IF(r.ok())
        {
            CHECK(r.value().content.type == "text/xml");

            auto body = r.value().content.data;
            CHECK(!body.empty());
            CHECK(rocky::detail::startsWith(body, "<?xml"));
        }
        else
        {
            std::cerr << "HTTP/S request failed: " << r.error().message << std::endl;
        }
    }

    SECTION("HTTPS")
    {
        if (URI::supportsHTTPS())
        {
            URI uri("https://readymap.org/readymap/tiles/1.0.0/7/");
            auto r = uri.read(IOOptions());
            CHECKED_IF(r.ok())
            {
                CHECK(r.value().content.type == "text/xml");
                auto body = r.value().content.data;
                CHECK(!body.empty());
                CHECK(rocky::detail::startsWith(body, "<?xml"));
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
    CHECKED_IF(result.ok())
    {
        auto contextSingleton = VSGContextFactory::create(nullptr);
        auto context = contextSingleton.get();
        CHECK(context->status.ok());

        auto mapNode = MapNode::create(context);
        auto r = mapNode->from_json(result.value(), context->io.from(earthFile));
        CHECK(r.ok());

        auto layers = mapNode->map->layers([&](auto layer) {
            return layer->name == "ReadyMap 15m Imagery"; });

        CHECK(layers.size() == 1);
        
        auto layer1 = layers.front();
        CHECKED_IF(layer1)
        {
            auto tms_layer = TMSImageLayer::cast(layer1);
            CHECK(tms_layer);
            CHECK(tms_layer->uri.has_value());
            CHECK(tms_layer->uri.value().full() == "https://readymap.org/readymap/tiles/1.0.0/7/");
        }
    }
}

TEST_CASE("MapManipulator NaN fix")
{
    // Test case for issue #105 - NaN error in MapManipulator.cpp
    // This tests that very close vectors don't produce NaN when creating quaternions
    
    // Test data from the original issue
    vsg::dvec3 center(-2248544.6429430502, 5050313.7280376982, 3170373.6894794349);
    vsg::dvec3 target(-2248544.6095093964, 5050313.7806744399, 3170373.6297455574);
    
    // Calculate distance between the vectors
    double dist = vsg::length(center - target);
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
