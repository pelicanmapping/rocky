/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TransformSystem.h"
#include "TransformData.h"
#include <rocky/vsg/Utils.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    void on_construct_Transform(entt::registry& r, entt::entity e)
    {
        auto& data = r.emplace<TransformData>(e);
        auto& transform = r.get<Transform>(e);
        data.transform = &transform;
    }

    void on_update_Transform(entt::registry& r, entt::entity e)
    {
        auto& transform = r.get<Transform>(e);
        auto& data = r.get<TransformData>(e);
        data.transform = &transform;
    }

    void on_destroy_Transform(entt::registry& r, entt::entity e)
    {
        r.remove<TransformData>(e);
    }
}

TransformSystem::TransformSystem(ecs::Registry& r) : ecs::System(r)
{
    // configure EnTT to automatically add the necessary components when a Transform is constructed
    auto [lock, registry] = _registry.write();

    // Each Transform component automatically gets a TransformData component
    // that tracks internal per-view transform information.
    registry.on_construct<Transform>().connect<&on_construct_Transform>();
    registry.on_update<Transform>().connect<&on_update_Transform>();
    registry.on_destroy<Transform>().connect<&on_destroy_Transform>();
}

void
TransformSystem::traverse(vsg::RecordTraversal& record) const
{
    auto [lock, registry] = _registry.read();

    registry.view<TransformData>().each([&](auto& transformData)
        {
            transformData.update(record);
        });
}
