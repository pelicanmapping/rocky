/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        static constexpr const char* RENDER_DOMAIN_KEY = "rocky.render_domain";
        static constexpr const char* OVERLAY_BAKE_TARGET_KEY = "rocky.overlay_bake.target";

        enum RenderDomain
        {
            RenderDomain_Main = 0,
            RenderDomain_OverlayBake = 1
        };
    }
}
