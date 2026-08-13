/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <entt/entt.hpp>
#include <utility>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        static constexpr const char* RENDER_DOMAIN_KEY = "rocky.render_domain";
        static constexpr const char* OVERLAY_BAKE_TARGET_KEY = "rocky.overlay_bake.target";

        enum class RenderDomain
        {
            Main,
            OverlayBake
        };

        template<typename T>
        inline std::pair<RenderDomain, entt::entity> getRenderDomainAndOverlayTarget(const T& object)
        {
            RenderDomain renderDomain = RenderDomain::Main;
            entt::entity overlayTarget = entt::null;

            if (!object.getValue(RENDER_DOMAIN_KEY, renderDomain))
            {
                int rawDomain = static_cast<int>(RenderDomain::Main);
                if (object.getValue(RENDER_DOMAIN_KEY, rawDomain))
                {
                    renderDomain = (rawDomain == static_cast<int>(RenderDomain::OverlayBake)) ?
                        RenderDomain::OverlayBake :
                        RenderDomain::Main;
                }
            }

            if (renderDomain == RenderDomain::OverlayBake)
                object.getValue(OVERLAY_BAKE_TARGET_KEY, overlayTarget);

            return { renderDomain, overlayTarget };
        }
    }
}
