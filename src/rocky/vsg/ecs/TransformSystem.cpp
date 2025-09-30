/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TransformSystem.h"
#include "TransformDetail.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    void on_construct_Transform(entt::registry& r, entt::entity e)
    {
        r.emplace<TransformDetail>(e);
    }

    void on_update_Transform(entt::registry& r, entt::entity e)
    {
        (void)r.get_or_emplace<TransformDetail>(e);
    }

    void on_destroy_Transform(entt::registry& r, entt::entity e)
    {
        r.remove<TransformDetail>(e);
    }
}

TransformSystem::TransformSystem(Registry& r) : System(r)
{
    // configure EnTT to automatically add the necessary components when a Transform is constructed
    auto [lock, registry] = r.write();

    // Each Transform component automatically gets a TransformDetail component
    // that tracks internal per-view transform information.
    registry.on_construct<Transform>().connect<&on_construct_Transform>();
    registry.on_update<Transform>().connect<&on_update_Transform>();
    registry.on_destroy<Transform>().connect<&on_destroy_Transform>();
}

void
TransformSystem::update(VSGContext& context)
{
    auto [lock, registry] = _registry.read();

    for(auto&& [entity, transform, detail] : registry.view<Transform, TransformDetail>().each())
    {
        if (transform.revision != detail.sync.revision)
        {
            detail.sync = transform;
        }
    }
}

void
TransformSystem::traverse(vsg::RecordTraversal& record) const
{
    auto [lock, registry] = _registry.read();

    bool something_changed = false;

    registry.view<TransformDetail>().each([&](auto& transform_detail)
        {
            something_changed = transform_detail.update(record) || something_changed;
        });

    if (something_changed && onChanges)
    {
        onChanges.fire();
    }
}
