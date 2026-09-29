/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/vsg/ecs/DecalSystem.h>
#include <rocky/vsg/ecs/TextureSystem.h>
#include <vsg/utils/ShaderCompiler.h>
#include <filesystem>
#include <thread>
#include <type_traits>
#include <vector>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

//! A view may be collected repeatedly; initialization must be empty and must not duplicate or reset published data.
TEST_CASE("view dependent decal state is empty and initializes only once", "[projection][views]")
{
    auto view = vsg::View::create();
    auto state = ViewDependentStateEx::create(view, vsg::ref_ptr<vsg::Device>{});
    view->viewDependentState = state;
    vsg::ResourceRequirements requirements;
    state->init(requirements);

    const auto descriptorCount = state->descriptorSet->descriptors.size();
    const auto bindingCount = state->descriptorSetLayout->bindings.size();
    const auto descriptorSet = state->descriptorSet;
#ifdef ROCKY_HAS_DECALS
    BufferAccess<DecalGPU> decals(state->decalsBuf);
    REQUIRE(decals.capacity() == 1u);
    CHECK(decals->count == 0);
    decals->count = 7; // Stand in for a header already published by DecalSystem.
#endif

    state->init(requirements);
    CHECK(state->descriptorSet == descriptorSet);
    CHECK(state->descriptorSet->descriptors.size() == descriptorCount);
    CHECK(state->descriptorSetLayout->bindings.size() == bindingCount);
#ifdef ROCKY_HAS_DECALS
    CHECK(BufferAccess<DecalGPU>(state->decalsBuf)->count == 7);
#endif
}

//! Compile decal culling, shading, and volume diagnostics with and without vector support, without requiring a GPU.
TEST_CASE("decal culling and terrain shaders compile together", "[projection][shader]")
{
    auto compiler = vsg::ShaderCompiler::create();
    if (!compiler->supported())
    {
        WARN("This VSG build does not include a shader compiler");
        return;
    }

    std::vector<std::string> defines;
    SECTION("raster only") { }
    SECTION("vector enabled") { defines.emplace_back("ROCKY_HAS_SLUGHORN"); }

    const auto shadersPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "rocky/vsg/shaders";
    auto options = vsg::Options::create();
    options->paths.push_back(shadersPath.string());
    auto compute = vsg::ShaderStage::read(
        VK_SHADER_STAGE_COMPUTE_BIT, "main", (shadersPath / "rocky.decal.cull.comp").string(), options);
    REQUIRE(compute);
    REQUIRE(compiler->compile(compute, defines, options));
    CHECK_FALSE(compute->module->code.empty());

    auto vertex = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT, "main", (shadersPath / "rocky.terrain.vert").string(), options);
    auto fragment = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", (shadersPath / "rocky.terrain.frag").string(), options);
    REQUIRE(vertex);
    REQUIRE(fragment);
    vsg::ShaderStages shaders{ vertex, fragment };
    REQUIRE(compiler->compile(shaders, defines, options));
    CHECK_FALSE(vertex->module->code.empty());
    CHECK_FALSE(fragment->module->code.empty());

    auto debugVertex = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT, "main", (shadersPath / "rocky.decal.debug.vert").string(), options);
    auto debugFragment = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main", (shadersPath / "rocky.decal.debug.frag").string(), options);
    REQUIRE(debugVertex);
    REQUIRE(debugFragment);
    vsg::ShaderStages debugShaders{ debugVertex, debugFragment };
    REQUIRE(compiler->compile(debugShaders, defines, options));
    CHECK_FALSE(debugVertex->module->code.empty());
    CHECK_FALSE(debugFragment->module->code.empty());
}

//! Debug geometry stays renderer-owned, uses line primitives without vertex arrays, and never writes scene depth.
TEST_CASE("decal volume diagnostics own bounded reusable graphics state", "[projection][decal-debug]")
{
    Registry registry = Registry::create();
    auto system = DecalSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    const auto shaderRoot = std::filesystem::path(__FILE__).parent_path().parent_path() / "rocky/vsg";
    context.get()->searchPaths.push_back(shaderRoot.string());
    auto node = system->debugNode();
    REQUIRE(node);
    CHECK_FALSE(system->debugVolumes);
    CHECK(system->debugVolumesSeeThrough);

    // Reinitialization must replace, not accumulate, the two pipeline variants.
    for (unsigned initialization = 0; initialization != 2; ++initialization)
    {
        system->initialize(context.get());
        REQUIRE(system->debugNode() == node);
        auto* group = dynamic_cast<vsg::Group*>(node.get());
        REQUIRE(group);
        REQUIRE(group->children.size() == 2u);
        for (unsigned variant = 0; variant != 2; ++variant)
        {
            auto* stateGroup = dynamic_cast<vsg::StateGroup*>(group->children[variant].get());
            REQUIRE(stateGroup);
            REQUIRE(stateGroup->stateCommands.size() == 2u);
            auto* bind = dynamic_cast<vsg::BindGraphicsPipeline*>(stateGroup->stateCommands.front().get());
            REQUIRE(bind);
            bool foundAssembly = false, foundDepth = false, foundVertexInput = false;
            for (const auto& state : bind->pipeline->pipelineStates)
            {
                if (auto* assembly = dynamic_cast<vsg::InputAssemblyState*>(state.get()))
                {
                    foundAssembly = true;
                    CHECK(assembly->topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
                }
                if (auto* depth = dynamic_cast<vsg::DepthStencilState*>(state.get()))
                {
                    foundDepth = true;
                    CHECK(depth->depthWriteEnable == VK_FALSE);
                    CHECK(depth->depthTestEnable == (variant ? VK_FALSE : VK_TRUE));
                }
                if (auto* input = dynamic_cast<vsg::VertexInputState*>(state.get()))
                {
                    foundVertexInput = true;
                    CHECK(input->vertexBindingDescriptions.empty());
                    CHECK(input->vertexAttributeDescriptions.empty());
                }
            }
            CHECK(foundAssembly);
            CHECK(foundDepth);
            CHECK(foundVertexInput);
        }
    }
}

//! Keeps the existing system alias bound to internal ECS storage and verifies
//! that moving its type does not change dirty notification or queue draining.
TEST_CASE("component dirty queues use internal storage", "[ecs][projection]")
{
    static_assert(std::is_same_v<Overlay::Dirty, detail::ComponentDirty<Overlay>>);
    static_assert(!std::is_same_v<Overlay::Dirty, ImageTexture::Dirty>);

    entt::registry registry;
    const auto queue = registry.create();
    registry.emplace<Overlay::Dirty>(queue);
    REQUIRE(registry.all_of<detail::ComponentDirty<Overlay>>(queue));

    const auto entity = registry.create();
    registry.emplace<Overlay>(entity);
    Overlay::dirty(registry, entity);
    CHECK(registry.get<detail::ComponentDirty<Overlay>>(queue).entities.size() == 1u);

    unsigned notifications = 0u;
    Overlay::eachDirty(registry, [&](entt::entity changed)
    {
        CHECK(changed == entity);
        ++notifications;
    });
    CHECK(notifications == 1u);
    CHECK(registry.get<detail::ComponentDirty<Overlay>>(queue).entities.empty());
}

TEST_CASE("image texture destruction defers producer-owned images", "[projection][texture-lifetime]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    std::vector<vsg::ref_ptr<vsg::Object>> retired;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    context.get()->disposer = [&](vsg::ref_ptr<vsg::Object> object)
    {
        retired.emplace_back(std::move(object));
    };

    entt::entity entity = entt::null;
    vsg::observer_ptr<vsg::ImageInfo> image;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<ImageTexture>(entity);
        auto& resource = reg.emplace<TextureResource>(entity);
        resource.producer = TextureResourceProducer::ImageTexture;
        // No Vulkan device is needed to test the ownership/disposal handoff.
        resource.texture = vsg::ImageInfo::create();
        image = resource.texture;
    });

    SECTION("remove the image component")
    {
        registry.write([&](entt::registry& reg) { reg.remove<ImageTexture>(entity); });
    }
    SECTION("remove the resource before its source component")
    {
        registry.write([&](entt::registry& reg) { reg.remove<TextureResource>(entity); });
    }
    SECTION("destroy the entire entity")
    {
        registry.write([&](entt::registry& reg) { reg.destroy(entity); });
    }
    SECTION("destroy on a paging thread")
    {
        std::thread worker([&]()
        {
            registry.write([&](entt::registry& reg) { reg.destroy(entity); });
        });
        worker.join();
    }
    SECTION("drain destruction after a system failure")
    {
        textureSystem->status = Failure(Failure::ResourceUnavailable, "Test failure");
        registry.write([&](entt::registry& reg) { reg.destroy(entity); });
    }

    // Removal must keep the image alive without invoking VSG on the removing
    // thread. The next update hands it off exactly once to the GPU disposer.
    CHECK(image.ref_ptr().valid());
    CHECK(retired.empty());
    textureSystem->update(context.get());
    REQUIRE(retired.size() == 1u);
    {
        auto batch = retired.front().cast<vsg::Objects>();
        REQUIRE(batch);
        REQUIRE(batch->children.size() == 1u);
        CHECK(batch->children.front().get() == image.ref_ptr().get());
    }
    textureSystem->update(context.get());
    CHECK(retired.size() == 1u);
    CHECK(image.ref_ptr().valid());
    retired.clear();
    CHECK_FALSE(image.ref_ptr().valid());
}

TEST_CASE("image texture disposal leaves other producers alone", "[projection][texture-lifetime]")
{
    Registry registry = Registry::create();
    auto textureSystem = TextureSystemNode::create(registry);
    unsigned disposalCount = 0u;
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    context.get()->disposer = [&](vsg::ref_ptr<vsg::Object>) { ++disposalCount; };

    auto producer = TextureResourceProducer::External;
    SECTION("external resource") { producer = TextureResourceProducer::External; }
    SECTION("render texture resource") { producer = TextureResourceProducer::RenderTexture; }

    vsg::observer_ptr<vsg::ImageInfo> image;
    registry.write([&](entt::registry& reg)
    {
        auto entity = reg.create();
        reg.emplace<ImageTexture>(entity);
        auto& resource = reg.emplace<TextureResource>(entity);
        resource.producer = producer;
        resource.texture = vsg::ImageInfo::create();
        image = resource.texture;
        reg.remove<ImageTexture>(entity);
        CHECK(reg.any_of<TextureResource>(entity));
        reg.destroy(entity);
    });
    CHECK_FALSE(image.ref_ptr().valid());
    textureSystem->update(context.get());
    CHECK(disposalCount == 0u);
}

TEST_CASE("projected texture dirty queues stay bounded", "[projection]")
{
    Registry registry = Registry::create();
    auto decalSystem = DecalSystemNode::create(registry);
    auto context = VSGContextFactory::create(vsg::Viewer::create());
    // Loads the shader source but does not compile or allocate Vulkan objects:
    // this headless context has no views, textures, or device to render with.
    decalSystem->initialize(context.get());
    REQUIRE(decalSystem->status.ok());

    entt::entity entity = entt::null;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<ProjectedTexture>(entity);
    });
    for (unsigned frame = 0u; frame < 8u; ++frame)
    {
        std::uint64_t revision = 0u;
        registry.write([&](entt::registry& reg)
        {
            auto& projected = reg.get<ProjectedTexture>(entity);
            for (unsigned update = 0u; update < 64u; ++update)
                projected.dirty(reg);
            revision = projected.componentRevision();
            auto removed = reg.create();
            reg.emplace<ProjectedTexture>(removed).dirty(reg);
            reg.destroy(removed);
            reg.view<ProjectedTexture::Dirty>().each([](auto& dirty)
            {
                CHECK(dirty.entities.size() == 65u);
            });
        });
        decalSystem->update(context.get());
        registry.read([&](entt::registry& reg)
        {
            CHECK(reg.get<ProjectedTexture>(entity).componentRevision() == revision);
            reg.view<ProjectedTexture::Dirty>().each([](const auto& dirty)
            {
                CHECK(dirty.entities.empty());
            });
        });
    }
}
