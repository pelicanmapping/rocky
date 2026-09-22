/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "catch.hpp"
#include <rocky/vsg/ecs/DecalSystem.h>
#include <rocky/vsg/ecs/TextureSystem.h>
#include <thread>
#include <type_traits>
#include <vector>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

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
