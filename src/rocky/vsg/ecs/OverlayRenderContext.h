/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <entt/entt.hpp>
#include <utility>
#include <algorithm>

namespace ROCKY_NAMESPACE
{
    namespace detail
    {
        static constexpr const char* RENDER_PURPOSE_KEY = "rocky.render_purpose";
        static constexpr const char* RENDER_REQUEST_KEY = "rocky.render_request";

        // Kept temporarily so third-party record traversals using the old keys
        // remain source-compatible while migrating to RenderRequest.
        static constexpr const char* RENDER_DOMAIN_KEY = RENDER_PURPOSE_KEY;
        static constexpr const char* OVERLAY_BAKE_TARGET_KEY = "rocky.overlay_bake.target";

        enum class RenderPurpose
        {
            Main,
            RenderTexture,
            OverlayBake = RenderTexture
        };

        using RenderDomain = RenderPurpose;

        struct RenderRequest
        {
            RenderPurpose purpose = RenderPurpose::Main;
            entt::entity controller = entt::null;
            std::vector<entt::entity> sources;

            //! Used only by the legacy same-entity Overlay adapter. Native
            //! RenderTexture jobs keep source and projector transforms separate.
            bool ignoreSourceTransforms = false;

            bool contains(entt::entity entity) const
            {
                return purpose == RenderPurpose::Main ||
                    (sources.empty() ? entity == controller :
                        std::find(sources.begin(), sources.end(), entity) != sources.end());
            }
        };

        template<typename T>
        inline RenderRequest getRenderRequest(const T& object)
        {
            RenderRequest request;
            if (object.getValue(RENDER_REQUEST_KEY, request))
                return request;

            if (!object.getValue(RENDER_PURPOSE_KEY, request.purpose))
            {
                int rawDomain = static_cast<int>(RenderPurpose::Main);
                if (object.getValue(RENDER_DOMAIN_KEY, rawDomain))
                {
                    request.purpose = (rawDomain == static_cast<int>(RenderPurpose::RenderTexture)) ?
                        RenderPurpose::RenderTexture :
                        RenderPurpose::Main;
                }
            }

            if (request.purpose == RenderPurpose::RenderTexture)
            {
                object.getValue(OVERLAY_BAKE_TARGET_KEY, request.controller);
                if (request.controller != entt::null)
                    request.sources.emplace_back(request.controller);
            }

            return request;
        }

        template<typename T>
        inline std::pair<RenderDomain, entt::entity> getRenderDomainAndOverlayTarget(const T& object)
        {
            auto request = getRenderRequest(object);
            return { request.purpose, request.controller };
        }
    }
}
