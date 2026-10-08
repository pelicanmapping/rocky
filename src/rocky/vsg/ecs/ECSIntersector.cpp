/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "ECSIntersector.h"
#include "ECSVisitors.h"
#include "DecalSystem.h"
#include <rocky/vsg/DisplayManager.h>
#include <algorithm>
#include <cmath>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Scopes entity markers to their scene branches so receiver/UI geometry cannot inherit the previous entity.
    class GeometryIntersector : public detail::ECSPolytopeIntersector
    {
    public:
        using detail::ECSPolytopeIntersector::ECSPolytopeIntersector;

        //! Preserves VSG's traversal/state handling while restoring the enclosing entity after a node traversal.
        void apply(const vsg::Node& node) override
        {
            const auto saved = currentEntity;
            detail::ECSPolytopeIntersector::apply(node);
            currentEntity = saved;
        }
    };
}

ECSIntersector::Results
ECSIntersector::intersect(View& view, int x, int y, int pixelTolerance) const
{
    Results results;
    if (!view || !view.vsgView->camera)
        return results;
    const auto& camera = *view.vsgView->camera;
    if (!camera.viewMatrix || !camera.projectionMatrix)
        return results;
    const auto viewport = camera.getViewport();
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
        !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0.0f || viewport.height <= 0.0f ||
        double(x) < viewport.x || double(y) < viewport.y ||
        double(x) >= double(viewport.x) + viewport.width || double(y) >= double(viewport.y) + viewport.height)
        return results;

    // Reject a replaced raw vsgView until setup refreshes its association; never use another view's registry.
    if (view._pickingView != view.vsgView)
        return results;
    auto ecs = view._ecs.ref_ptr();
    if (ecs)
    {
        // Preserve the established buffered geometry query. Double arithmetic avoids integer overflow
        // when callers supply extreme coordinates or tolerances; negative tolerances act as zero.
        const double radius = std::max(0, pixelTolerance);
        GeometryIntersector geometry(view.vsgView, x - radius, y - radius, x + radius, y + radius);
        view.vsgView->traverse(geometry);
        results = std::move(geometry.collectedEntities);
        ecs->registry.read([&](entt::registry& reg)
        {
            for (auto hit = results.begin(); hit != results.end();)
            {
                const auto entity = *hit;
                const auto* participation = reg.valid(entity) ? reg.try_get<RenderParticipation>(entity) : nullptr;
                // Projected sources are evaluated on their receiver, including when a custom visitor reports them.
                if (!reg.valid(entity) || reg.any_of<Overlay, ProjectedTexture>(entity) ||
                    (participation && !participation->mainView))
                    hit = results.erase(hit);
                else
                    ++hit;
            }
        });
    }

    // An expired compute branch disables vector picking even if its system survives elsewhere. No graph search here.
    if (auto compute = view._computeGraph.ref_ptr())
    {
        if (auto decals = view._decalSystem.ref_ptr())
        {
            const auto entity = decals->intersectVectorOverlay(view, x, y, 0.01f);
            if (entity != entt::null)
                results.emplace(entity);
        }
    }
    return results;
}
