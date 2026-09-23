/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/ecs/Texture.h>
#include <rocky/vsg/ecs/Texture.h>
#include <rocky/vsg/ecs/TextureSystem.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include <rocky/vsg/ecs/PolygonSystem.h>
#include <vsg/utils/ShaderCompiler.h>
#include <filesystem>
#include <type_traits>
#include <thread>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

//! Native and CPU producers publish through the same resource contract, including sources created before the system.
TEST_CASE("shared texture publication and replacement", "[texture][ecs]")
{
    static_assert(std::is_same_v<MeshTexture, Texture>);
    static_assert(std::is_same_v<MeshTexture::Dirty, Texture::Dirty>);
    Registry registry = Registry::create();
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    auto supplied = vsg::ImageInfo::create();
    entt::entity entity;
    bool native = false;
    SECTION("native texture") { native = true; }
    SECTION("CPU image texture") { native = false; }
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        if (native)
            reg.emplace<Texture>(entity).imageInfo = supplied;
        else
            reg.emplace<ImageTexture>(entity).image = Image::create(Image::R8G8B8A8_UNORM, 2, 2);
    });
    auto textures = TextureSystemNode::create(registry);
    textures->update(context.get());
    vsg::ref_ptr<vsg::ImageInfo> first;
    std::uint64_t revision = 0u;
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<TextureResource>(entity);
        REQUIRE(resource.ready);
        REQUIRE(resource.texture);
        CHECK(resource.producer == (native ? TextureResourceProducer::Texture : TextureResourceProducer::ImageTexture));
        if (native)
            CHECK(resource.texture == supplied);
        first = resource.texture;
        revision = resource.revision;
    });
    for (int i = 0; i < 10; ++i)
        textures->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        CHECK(reg.get<TextureResource>(entity).texture == first);
        CHECK(reg.get<TextureResource>(entity).revision == revision);
        if (native)
            reg.patch<Texture>(entity, [](auto& texture) { texture.origin = TextureOrigin::UpperLeft; });
        else
            reg.patch<ImageTexture>(entity, [](auto& texture) { texture.origin = TextureOrigin::UpperLeft; });
    });
    textures->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        CHECK(reg.get<TextureResource>(entity).origin == TextureOrigin::UpperLeft);
        CHECK(reg.get<TextureResource>(entity).revision > revision);
        if (native)
        {
            // The compatibility alias addresses the same EnTT component.
            CHECK(&reg.get<MeshTexture>(entity) == &reg.get<Texture>(entity));
            reg.get<Texture>(entity).imageInfo = vsg::ImageInfo::create();
            supplied = reg.get<Texture>(entity).imageInfo;
        }
        else
            reg.get<ImageTexture>(entity).image = Image::create(Image::R8G8B8A8_UNORM, 4, 4);
    });
    textures->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        const auto& resource = reg.get<TextureResource>(entity);
        CHECK(resource.texture != first);
        if (native)
            CHECK(resource.texture == supplied);
        if (native)
            reg.get<Texture>(entity).imageInfo = {};
        else
            reg.get<ImageTexture>(entity).image.reset();
    });
    textures->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        const auto& resource = reg.get<TextureResource>(entity);
        CHECK(resource.ready);
        CHECK_FALSE(resource.texture);
        reg.view<Texture::Dirty>().each([](const auto& dirty) { CHECK(dirty.entities.empty()); });
        reg.view<ImageTexture::Dirty>().each([](const auto& dirty) { CHECK(dirty.entities.empty()); });
        reg.view<TextureResource::Dirty>().each([](const auto& dirty) { CHECK(dirty.entities.empty()); });
    });
}

//! Ambiguous input withdraws only this system's resource and recovers after the conflicting source is removed.
TEST_CASE("shared texture rejects conflicting producers", "[texture][ecs]")
{
    Registry registry = Registry::create();
    auto textures = TextureSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity entity;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<Texture>(entity).imageInfo = vsg::ImageInfo::create();
    });
    textures->update(context.get());
    bool renderTexture = false;
    SECTION("another image producer") { renderTexture = false; }
    SECTION("a render job") { renderTexture = true; }
    registry.write([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<TextureResource>(entity));
        if (renderTexture)
            reg.emplace<RenderTexture>(entity);
        else
            reg.emplace<ImageTexture>(entity);
    });
    textures->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        CHECK_FALSE(reg.any_of<TextureResource>(entity));
        if (renderTexture)
            reg.remove<RenderTexture>(entity);
        else
            reg.remove<ImageTexture>(entity);
    });
    textures->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.any_of<TextureResource>(entity));
        CHECK(reg.get<TextureResource>(entity).texture == reg.get<Texture>(entity).imageInfo);
    });
}

//! Native sources use the same paging-thread-safe deferred retirement as CPU image sources.
TEST_CASE("native texture destruction defers its published reference", "[texture][texture-lifetime]")
{
    Registry registry = Registry::create();
    auto textures = TextureSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    std::vector<vsg::ref_ptr<vsg::Object>> retired;
    context.get()->disposer = [&](auto object) { retired.emplace_back(std::move(object)); };
    entt::entity entity;
    vsg::observer_ptr<vsg::ImageInfo> image;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        auto& texture = reg.emplace<Texture>(entity);
        texture.imageInfo = vsg::ImageInfo::create();
        image = texture.imageInfo;
    });
    textures->update(context.get());
    SECTION("remove component")
    {
        registry.write([&](auto& reg) { reg.template remove<Texture>(entity); });
    }
    SECTION("destroy entity on paging thread")
    {
        std::thread worker([&]() { registry.write([&](auto& reg) { reg.destroy(entity); }); });
        worker.join();
    }
    CHECK(image.ref_ptr().valid());
    CHECK(retired.empty());
    textures->update(context.get());
    REQUIRE(retired.size() == 1u);
    textures->update(context.get());
    CHECK(retired.size() == 1u);
    retired.clear();
    CHECK_FALSE(image.ref_ptr().valid());
}

//! Texture flags follow the publication, not merely a non-null style reference.
TEST_CASE("mesh texture readiness and sampling flags", "[texture][ecs]")
{
    MeshStyle style;
    style.texture = static_cast<entt::entity>(7u);
    MeshStyleUniform uniform;
    uniform.populate(style);
    CHECK((uniform.featureMask & 1u) == 0u);
    TextureResource resource;
    resource.texture = vsg::ImageInfo::create();
    resource.origin = TextureOrigin::UpperLeft;
    resource.alphaMode = TextureAlphaMode::Premultiplied;
    uniform.populate(style, &resource);
    CHECK((uniform.featureMask & 25u) == 25u);
    MeshStyleDetail cached;
    CHECK_FALSE(cached.textureMatches(style.texture, &resource));
    cached.texture = style.texture;
    cached.textureImage = resource.texture.get();
    cached.textureOrigin = resource.origin;
    cached.textureAlphaMode = resource.alphaMode;
    CHECK(cached.textureMatches(style.texture, &resource));
    resource.ready = false;
    CHECK_FALSE(cached.textureMatches(style.texture, &resource));
    uniform.populate(style, &resource);
    CHECK((uniform.featureMask & 1u) == 0u);
    resource.ready = true;
    ++resource.revision;
    CHECK_FALSE(cached.textureMatches(style.texture, &resource));
    CHECK_FALSE(cached.textureMatches(style.texture, nullptr));
}

//! Polygon adapters share the texture entity and retire only derived geometry/style, never the supplied texture.
TEST_CASE("polygon forwards a shared texture without owning it", "[texture][polygon]")
{
    Registry registry = Registry::create();
    auto textures = TextureSystemNode::create(registry);
    auto polygons = PolygonSystemNode::create(registry);
    auto meshes = MeshSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    entt::entity texture, polygon;
    registry.write([&](entt::registry& reg)
    {
        texture = reg.create();
        reg.emplace<Texture>(texture).imageInfo = vsg::ImageInfo::create();
        polygon = reg.create();
        auto& geometry = reg.emplace<PolygonGeometry>(polygon);
        geometry.polygons.push_back(PolygonPart{ {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {} });
        auto& style = reg.emplace<PolygonStyle>(polygon);
        style.texture = texture;
        reg.emplace<rocky::Polygon>(polygon, geometry, style);
    });
    textures->update(context.get());
    polygons->update(context.get());
    entt::entity meshGeometry, meshStyle;
    std::uint64_t geometryRevision;
    RenderTextureRevision beforeMesh, beforePolygon;
    registry.write([&](entt::registry& reg)
    {
        const auto& mesh = reg.get<Mesh>(polygon);
        meshGeometry = mesh.geometry;
        meshStyle = mesh.style;
        geometryRevision = reg.get<MeshGeometry>(meshGeometry).componentRevision();
        CHECK(reg.get<MeshStyle>(meshStyle).texture == texture);
        meshes->contributeRenderTextureRevision(reg, polygon, beforeMesh);
        polygons->contributeRenderTextureRevision(reg, polygon, beforePolygon);
        reg.get<Texture>(texture).dirty(reg);
    });
    textures->update(context.get());
    polygons->update(context.get());
    registry.write([&](entt::registry& reg)
    {
        RenderTextureRevision afterMesh, afterPolygon;
        meshes->contributeRenderTextureRevision(reg, polygon, afterMesh);
        polygons->contributeRenderTextureRevision(reg, polygon, afterPolygon);
        CHECK(afterMesh.content != beforeMesh.content);
        CHECK(afterPolygon.content != beforePolygon.content);
        CHECK(afterMesh.bounds == beforeMesh.bounds);
        CHECK(afterPolygon.bounds == beforePolygon.bounds);
        CHECK(reg.get<MeshGeometry>(meshGeometry).componentRevision() == geometryRevision);
        reg.destroy(polygon);
        CHECK_FALSE(reg.valid(meshGeometry));
        CHECK_FALSE(reg.valid(meshStyle));
        CHECK((reg.any_of<Texture, TextureResource>(texture)));
    });
}

//! Compile the actual mesh shader pair together to validate sampling flags and the stage interface without a GPU.
TEST_CASE("mesh texture shaders compile together", "[texture][shader]")
{
    auto compiler = vsg::ShaderCompiler::create();
    if (!compiler->supported())
    {
        WARN("This VSG build does not include a shader compiler");
        return;
    }
    const auto shadersPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "rocky/vsg/shaders";
    auto options = vsg::Options::create();
    options->paths.push_back(shadersPath.string());
    auto vertex = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT, "main", (shadersPath / "rocky.mesh.vert").string(), options);
    auto fragment = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", (shadersPath / "rocky.mesh.frag").string(), options);
    REQUIRE(vertex);
    REQUIRE(fragment);
    vsg::ShaderStages shaders{ vertex, fragment };
    REQUIRE(compiler->compile(shaders, {}, options));
    CHECK_FALSE(vertex->module->code.empty());
    CHECK_FALSE(fragment->module->code.empty());
}

//! Ordinary ECS geometry must have texture publication even without an Application or decal subsystem.
TEST_CASE("default ECS systems include shared texture publication", "[texture][ecs]")
{
    Registry registry = Registry::create();
    auto systems = ECSNode::create(registry, true);
    REQUIRE(systems->get<TextureSystemNode>() != nullptr);
}
