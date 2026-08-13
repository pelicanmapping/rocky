/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TransformSystem.h"
#include "TransformDetail.h"
#include "OverlayRenderContext.h"

using namespace ROCKY_NAMESPACE;


void TransformSystemNode::on_construct_Transform(entt::registry& r, entt::entity e)
{
    r.emplace<TransformDetail>(e);
}

void TransformSystemNode::on_update_Transform(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<TransformDetail>(e);
}

void TransformSystemNode::on_destroy_Transform(entt::registry& r, entt::entity e)
{
    r.remove<TransformDetail>(e);
}



TransformSystemNode::TransformSystemNode(Registry& r) : System(r)
{
    // configure EnTT to automatically add the necessary components when a Transform is constructed
    auto [lock, registry] = r.write();

    // Each Transform component automatically gets a TransformDetail component
    // that tracks internal per-view transform information.
    registry.on_construct<Transform>().connect<&TransformSystemNode::on_construct_Transform>(*this);
    registry.on_update<Transform>().connect<&TransformSystemNode::on_update_Transform>(*this);
    registry.on_destroy<Transform>().connect<&TransformSystemNode::on_destroy_Transform>(*this);
}

void
TransformSystemNode::update(VSGContext context)
{
    auto [lock, registry] = _registry.read();

    registry.view<Transform, TransformDetail>().each([context](auto& transform, auto& detail)
        {
            if (transform.revision != detail.sync.revision)
            {
                detail.sync = transform;
                detail.devicePixelRatio = context->devicePixelRatio();
            }
        });
}

void
TransformSystemNode::traverse(vsg::RecordTraversal& record) const
{
    auto [lock, registry] = _registry.read();

    // detect a profile/srs change so we can tell TransforDetail's to clear caches.
    bool srs_changed = false;
    auto viewID = record.getCommandBuffer()->viewID;
    auto& view = views[viewID];
    SRS worldSRS;
    if (record.getValue("rocky.worldsrs", worldSRS) && worldSRS != view.worldSRS)
    {
        view.worldSRS = worldSRS;
        srs_changed = true;
    }

    bool at_least_one_transform_changed = false;

    auto [renderDomain, overlayTarget] = detail::getRenderDomainAndOverlayTarget(record);

    if (renderDomain == detail::RenderDomain::OverlayBake && overlayTarget != entt::null)
    {
        if (auto* transformDetail = registry.try_get<TransformDetail>(overlayTarget))
        {
            if (srs_changed)
                transformDetail->reset(viewID);

            auto* pixelScale = registry.try_get<PixelScale>(overlayTarget);
            at_least_one_transform_changed = transformDetail->traverse(record, pixelScale);
        }
    }
    else
    {

        registry.view<TransformDetail, PixelScale>().each([&](auto& transform_detail, auto& pixel_scale)
        {
            if (srs_changed)
                transform_detail.reset(viewID);

            at_least_one_transform_changed = transform_detail.traverse(record, &pixel_scale)
                || at_least_one_transform_changed;
        });

        registry.view<TransformDetail>(entt::exclude<PixelScale>).each([&](auto& transform_detail)
        {
            if (srs_changed)
                transform_detail.reset(viewID);

            at_least_one_transform_changed = transform_detail.traverse(record, nullptr)
                || at_least_one_transform_changed;
        });
    }

    if (at_least_one_transform_changed && onChanges)
    {
        onChanges.fire();
    }
}
