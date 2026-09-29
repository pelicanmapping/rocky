/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/vsg/RTT.h>
#include <rocky/vsg/ecs/OverlayBakeSystem.h>
#include <rocky/vsg/ecs/DecalSystem.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include <rocky/vsg/ecs/OpticsSystem.h>
#include <atomic>
#include <filesystem>
#include <thread>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    //! Creates a device-free compile context for testing publication, not Vulkan allocation.
    auto makeBakeTestContext()
    {
        auto viewer = vsg::Viewer::create();
        viewer->compileManager = vsg::CompileManager::create(*viewer, vsg::ResourceHints::create());
        auto context = VSGContextFactory::create(viewer);
        context.get()->searchPaths.push_back(
            (std::filesystem::path(__FILE__).parent_path().parent_path() / "rocky/vsg").string());
        return context;
    }

    //! Installs a CPU-only job with the real bake-view wrapper, avoiding a Vulkan device.
    entt::entity addBakeJob(Registry& registry, vsg::ref_ptr<vsg::CommandGraph> host, vsg::ref_ptr<vsg::View> view)
    {
        entt::entity job;
        registry.write([&](entt::registry& reg)
        {
            job = reg.create();
            reg.emplace<RenderTexture>(job).fitToSources = false;
            auto& transform = reg.emplace<Transform>(job);
            transform.position = GeoPoint(SRS::ECEF, 1000.0, 2000.0, 3000.0);
            auto& detail = reg.emplace<OverlayBakeDetail>(job);
            detail.textureSize = { 512u, 512u };
            detail.fitToSources = false;
            detail.texture = vsg::ImageInfo::create();
            detail.renderGraph = vsg::RenderGraph::create();
            detail.viewNode = createOverlayBakeView(view, SRS::ECEF, detail.textureSize);
            detail.renderGraph->addChild(detail.viewNode);
            detail.hostCommandGraph = host;
            host->addChild(detail.renderGraph);
            auto& resource = reg.emplace<TextureResource>(job);
            resource.producer = TextureResourceProducer::RenderTexture;
            resource.texture = detail.texture;
            resource.ready = false;
        });
        return job;
    }
}

//! Reused color/depth attachments need global dependencies even when old pixels are discarded.
TEST_CASE("RTT dependencies protect sampling and rebaking", "[projection][rtt]")
{
    bool color = true, depth = false;
    SECTION("color") { }
    SECTION("color and depth") { depth = true; }
    SECTION("depth only") { color = false; depth = true; }
    const auto dependencies = makeRTTDependencies(color, depth);
    REQUIRE(dependencies.size() == (color ? 2u : 1u));
    const auto& incoming = dependencies.front();
    CHECK(incoming.srcSubpass == VK_SUBPASS_EXTERNAL);
    CHECK(incoming.dstSubpass == 0u);
    CHECK(incoming.srcStageMask == VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    CHECK(incoming.dependencyFlags == 0u);
    if (depth)
    {
        CHECK((incoming.srcAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0u);
        CHECK((incoming.dstAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0u);
        CHECK((incoming.dstStageMask & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT) != 0u);
        CHECK((incoming.dstStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) != 0u);
    }
    if (color)
    {
        CHECK((incoming.srcAccessMask & VK_ACCESS_SHADER_READ_BIT) != 0u);
        CHECK((incoming.srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0u);
        CHECK((incoming.dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0u);
        const auto& outgoing = dependencies.back();
        CHECK(outgoing.srcSubpass == 0u);
        CHECK(outgoing.dstSubpass == VK_SUBPASS_EXTERNAL);
        CHECK(outgoing.srcStageMask == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        CHECK(outgoing.dstStageMask == VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        CHECK(outgoing.srcAccessMask == VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        CHECK(outgoing.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
        CHECK(outgoing.dependencyFlags == 0u);
    }
    CHECK(makeRTTDependencies(false, false).empty());
}

//! Paging callbacks may retain resources, but only update may edit a live command graph.
TEST_CASE("raster job destruction retires on the update thread", "[projection][rtt][texture-lifetime]")
{
    auto registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    auto context = makeBakeTestContext();
    auto host = vsg::CommandGraph::create();
    auto view = vsg::View::create(vsg::Camera::create());
    auto job = addBakeJob(registry, host, view);
    std::vector<vsg::ref_ptr<vsg::Object>> retired;
    context.get()->disposer = [&](auto object) { retired.push_back(std::move(object)); };
    vsg::observer_ptr<vsg::ImageInfo> image;
    registry.read([&](entt::registry& reg) { image = reg.get<OverlayBakeDetail>(job).texture; });

    bool fail = false, wholeEntity = false;
    SECTION("remove producer") { }
    SECTION("destroy entity") { wholeEntity = true; }
    SECTION("drain after failure") { fail = true; }
    std::thread worker([&]()
    {
        registry.write([&](entt::registry& reg)
        {
            if (wholeEntity) reg.destroy(job);
            else reg.remove<RenderTexture>(job);
        });
    });
    worker.join();
    REQUIRE(host->children.size() == 1u);
    CHECK(retired.empty());
    CHECK(image.ref_ptr().valid());
    if (fail)
        bake->status = Failure(Failure::ResourceUnavailable, "Test failure");
    bake->update(context.get());
    CHECK(host->children.empty());
    REQUIRE(retired.size() == 1u);
    CHECK(image.ref_ptr().valid());
    bake->update(context.get());
    CHECK(retired.size() == 1u);
    retired.clear();
    CHECK_FALSE(image.ref_ptr().valid());
}

//! A component must not retain either the host scene or a shared bake view's ECS systems.
TEST_CASE("raster jobs have no registry ownership back edges", "[projection][rtt][texture-lifetime]")
{
    vsg::observer_ptr<vsg::ImageInfo> image;
    vsg::observer_ptr<vsg::View> observedView;
    vsg::observer_ptr<vsg::CommandGraph> observedHost;
    vsg::observer_ptr<MeshSystemNode> observedMesh;
    {
        auto registry = Registry::create();
        auto bake = OverlayBakeSystemNode::create(registry);
        auto mesh = MeshSystemNode::create(registry);
        auto host = vsg::CommandGraph::create();
        host->addChild(bake);
        auto view = vsg::View::create(vsg::Camera::create(), mesh);
        observedView = view;
        observedHost = host;
        observedMesh = mesh;
        auto job = addBakeJob(registry, host, view);
        registry.read([&](entt::registry& reg) { image = reg.get<OverlayBakeDetail>(job).texture; });
        // Deliberately leave the job alive when the surrounding owners go away.
    }
    CHECK_FALSE(observedView.ref_ptr().valid());
    CHECK_FALSE(observedHost.ref_ptr().valid());
    CHECK_FALSE(observedMesh.ref_ptr().valid());
    CHECK_FALSE(image.ref_ptr().valid());
}

//! Explicit shutdown is idempotent, preserves input components, and retires live targets before owners disappear.
TEST_CASE("raster shutdown detaches and withdraws live jobs", "[projection][rtt][texture-lifetime]")
{
    auto registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    auto context = makeBakeTestContext();
    auto host = vsg::CommandGraph::create();
    auto view = vsg::View::create(vsg::Camera::create());
    auto job = addBakeJob(registry, host, view);
    unsigned disposals = 0u;
    context.get()->disposer = [&](auto) { ++disposals; };
    bake->shutdown(context.get());
    CHECK(host->children.empty());
    CHECK(disposals == 1u);
    registry.read([&](entt::registry& reg)
    {
        CHECK(reg.all_of<RenderTexture>(job));
        CHECK_FALSE(reg.any_of<OverlayBakeDetail>(job));
        CHECK_FALSE(reg.any_of<TextureResource>(job));
    });
    bake->shutdown(context.get());
    CHECK(disposals == 1u);
    bake = {};
    // The registry can outlive the system; no callbacks may reference its dead instance.
    registry.write([&](entt::registry& reg) { reg.destroy(job); });
}

//! Rapid paged publication/removal exercises every unlocked interval without allocating targets.
TEST_CASE("raster update tolerates concurrent job removal", "[projection][rtt][threading]")
{
    auto registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    bake->bakeScene = {};
    auto context = makeBakeTestContext();
    std::atomic_bool finished{ false };
    std::thread worker([&]()
    {
        for (unsigned i = 0u; i != 1000u; ++i)
        {
            entt::entity job;
            registry.write([&](entt::registry& reg)
            {
                job = reg.create();
                reg.emplace<RenderTexture>(job);
            });
            std::this_thread::yield();
            registry.write([&](entt::registry& reg)
            {
                // The independently removable output can also disappear between phases.
                reg.remove<TextureResource>(job);
            });
            std::this_thread::yield();
            registry.write([&](entt::registry& reg) { reg.destroy(job); });
        }
        finished = true;
    });
    while (!finished)
        bake->update(context.get());
    worker.join();
    bake->update(context.get());
    CHECK(bake->status.ok());
    registry.read([&](entt::registry& reg) { CHECK(reg.view<RenderTexture>().empty()); });
}

//! A changed projector rebakes static sources, while a receiving-volume-only depth fit leaves pixels cached.
TEST_CASE("raster cache follows the bake projector not terrain depth", "[projection][rtt]")
{
    auto registry = Registry::create();
    auto bake = OverlayBakeSystemNode::create(registry);
    bake->worldSRS = SRS::ECEF;
    auto context = makeBakeTestContext();
    auto host = vsg::CommandGraph::create();
    auto view = vsg::View::create(vsg::Camera::create());
    auto job = addBakeJob(registry, host, view);
    registry.write([&](entt::registry& reg)
    {
        auto source = reg.create();
        reg.emplace<ActiveState>(source);
        reg.get<RenderTexture>(job).sources = { source };
    });
    for (unsigned i = 0; i != 4; ++i)
        bake->update(context.get());
    REQUIRE(host->children.empty());
    std::uint64_t generation;
    registry.write([&](entt::registry& reg)
    {
        REQUIRE(reg.get<RenderTextureStatus>(job).state == RenderTextureState::Ready);
        generation = reg.get<RenderTextureStatus>(job).generation;
        auto& transform = reg.get<Transform>(job);
        SECTION("position") { transform.position.x += 10.0; }
        SECTION("scale") { transform.localMatrix = glm::scale(glm::dmat4(1.0), glm::dvec3(2.0)); }
        SECTION("rotation") { transform.localMatrix = glm::rotate(glm::dmat4(1.0), 0.2, glm::dvec3(0, 0, 1)); }
        // No dirty call: the effective camera is the authoritative bake input.
    });
    bake->update(context.get());
    REQUIRE(host->children.size() == 1u);
    for (unsigned i = 0; i != 3; ++i)
        bake->update(context.get());
    CHECK(host->children.empty());
    registry.write([&](entt::registry& reg)
    {
        CHECK(reg.get<RenderTextureStatus>(job).generation == generation + 1u);
        auto& receiving = reg.emplace<ProjectionDetail>(job).views[0];
        receiving.terrainDepthRange = { -0.05, 0.05 };
        receiving.terrainDepthRangeValid = true;
    });
    bake->update(context.get());
    CHECK(host->children.empty());
    registry.read([&](entt::registry& reg)
    {
        CHECK(reg.get<RenderTextureStatus>(job).generation == generation + 1u);
    });
    bake->shutdown(context.get());
}

#ifdef ROCKY_HAS_DECALS
//! New, replaced, and recycled raster slots cannot publish in the frame that writes their descriptor.
TEST_CASE("raster descriptor residency waits for a later frame", "[projection][rtt][texture]")
{
    auto registry = Registry::create();
    auto decal = DecalSystemNode::create(registry);
    auto context = makeBakeTestContext();
    context.get()->sharedRenderData->configureProjectedTextureCapacity(1u);
    decal->initialize(context.get());
    REQUIRE(decal->status.ok());
    entt::entity entity;
    registry.write([&](entt::registry& reg)
    {
        entity = reg.create();
        reg.emplace<ProjectedTexture>(entity);
        reg.emplace<TextureResource>(entity).texture = vsg::ImageInfo::create();
    });
    auto* frame = context.get()->viewer()->getFrameStamp();
    REQUIRE(frame);
    frame->frameCount = 10u;
    decal->update(context.get());
    registry.read([&](entt::registry& reg)
    {
        REQUIRE(reg.get<TextureSlotDetail>(entity).descriptorImageIndex == 0);
        CHECK_FALSE(reg.get<TextureSlotDetail>(entity).readyForDraw);
    });
    decal->update(context.get()); // Multiple updates in the same frame are not a publication barrier.
    registry.read([&](entt::registry& reg) { CHECK_FALSE(reg.get<TextureSlotDetail>(entity).readyForDraw); });
    ++frame->frameCount;
    decal->update(context.get());
    registry.read([&](entt::registry& reg) { CHECK(reg.get<TextureSlotDetail>(entity).readyForDraw); });

    SECTION("replacement")
    {
        registry.write([&](entt::registry& reg)
        {
            reg.get<TextureResource>(entity).texture = vsg::ImageInfo::create();
        });
    }
    SECTION("recycled slot")
    {
        registry.write([&](entt::registry& reg)
        {
            reg.destroy(entity);
            entity = reg.create();
            reg.emplace<ProjectedTexture>(entity);
            reg.emplace<TextureResource>(entity).texture = vsg::ImageInfo::create();
        });
    }
    decal->update(context.get());
    registry.read([&](entt::registry& reg) { CHECK_FALSE(reg.get<TextureSlotDetail>(entity).readyForDraw); });
    ++frame->frameCount;
    decal->update(context.get());
    registry.read([&](entt::registry& reg) { CHECK(reg.get<TextureSlotDetail>(entity).readyForDraw); });
    CHECK(decal->status.ok());
}
#endif
