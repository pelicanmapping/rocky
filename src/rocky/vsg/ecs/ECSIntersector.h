/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>
#include <unordered_set>

namespace ROCKY_NAMESPACE
{
    class View;

    /**
     * Synchronous entity picking for a view's geometry and vector overlays.
     * Results contain all ordinary geometry hits plus at most the top contributing vector-overlay
     * instance. The set is deduplicated and unordered; it does not represent frontmost scene visibility.
     * Raster overlays and raster fallbacks are excluded. Geometry picking uses the internal polytope visitor.
     */
    class ROCKY_EXPORT ECSIntersector
    {
    public:
        using Results = std::unordered_set<entt::entity>;

        //! Creates a stateless picker reusable across views; dependencies are observed by each View.
        ECSIntersector() = default;

        //! Returns a fresh set for a window pixel in a view; invalid views/outside pixels return empty.
        //! pixelTolerance is a nonnegative geometry search radius (default 3); vector coverage uses the cursor pixel.
        //! Vector hits require at least 0.01 coverage times alpha and a current submitted projection on resident terrain.
        //! They do not test scene/raster occlusion; terrain footprints are approximate at silhouettes and discontinuities.
        //! Standalone views associate dependencies with View::setComputeGraph during graph assembly.
        //! Call on the application thread, serialized with ECS update, without holding a registry lock.
        Results intersect(View& view, int x, int y, int pixelTolerance = 3) const;

    };
}
