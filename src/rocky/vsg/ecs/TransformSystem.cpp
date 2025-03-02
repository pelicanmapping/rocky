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
    void on_construct_Transform(entt::registry & r, entt::entity e)
    {
        auto& views = r.emplace<TransformData>(e);
        auto& transform = r.get<Transform>(e);
        for (auto& view : views)
            view.transform = &transform;
    }
}

TransformSystem::TransformSystem(ecs::Registry& r) : ecs::System(r)
{
    // configure EnTT to automatically add the necessary components when a Transform is constructed
    auto [lock, registry] = _registry.write();
    registry.on_construct<Transform>().connect<&on_construct_Transform>();
}
