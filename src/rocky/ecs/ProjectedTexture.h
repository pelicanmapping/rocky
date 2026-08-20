/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Color.h>
#include <rocky/Image.h>
#include <rocky/GeoPoint.h>
#include <rocky/ecs/Component.h>
#include <vector>
#include <cfloat>
#include <algorithm>
#include <string>

namespace ROCKY_NAMESPACE
{
    //! Describes the coordinate convention used by a produced texture.
    enum class TextureOrigin
    {
        LowerLeft,
        UpperLeft
    };

    //! Describes how RGB is encoded relative to alpha in a produced texture.
    enum class TextureAlphaMode
    {
        Straight,
        Premultiplied
    };

    //! A CPU image that a renderer can publish as a TextureResource.
    struct ImageTexture : public Component<ImageTexture>
    {
        Image::Ptr image;
        TextureOrigin origin = TextureOrigin::LowerLeft;
        TextureAlphaMode alphaMode = TextureAlphaMode::Straight;
    };

    /**
     * Requests an offscreen rendering of one or more ECS entities.
     *
     * This component owns the render job, not necessarily the rendered
     * geometry. An empty sources collection means "render the owner".
     */
    struct RenderTexture : public Component<RenderTexture>
    {
        std::vector<entt::entity> sources;
        glm::uvec2 textureSize = { 512u, 512u };
        bool useDepthBuffer = false;
        bool continuous = false;

        //! If true, generate a projector Transform when the owner has none.
        bool fitToSources = true;
    };

    //! Observable lifecycle of a RenderTexture job.
    enum class RenderTextureState
    {
        WaitingForResources,
        WaitingForSources,
        Priming,
        Baking,
        Ready,
        Failed
    };

    struct RenderTextureStatus
    {
        RenderTextureState state = RenderTextureState::WaitingForResources;
        std::uint64_t generation = 0u;
        std::string message;
    };

    /**
     * Projects a texture through an ECS projector.
     *
     * Null references resolve to this component's owner. Keeping the texture,
     * projector, and projected instance separate permits one texture to be
     * reused by multiple projectors and one render job to consume many sources.
     */
    struct ProjectedTexture : public Component<ProjectedTexture>
    {
        entt::entity texture = entt::null;
        entt::entity projector = entt::null;
        Color color = StockColor::White;

        //! If true, projection is suppressed until the projector has Optics.
        //! This preserves the strict semantics of Decal::optics while native
        //! projected textures normally allow a Transform-only orthographic box.
        bool requireOptics = false;
    };

    /** Controls where an entity participates in rendering. */
    struct RenderParticipation : public Component<RenderParticipation>
    {
        bool mainView = true;
        bool renderTexture = true;
    };

    /**
     * Optional terrain policy for a projector.
     *
     * Optics describes a lens. TerrainClamp describes terrain-specific
     * behavior, so a generic projector need not know about terrain.
     */
    struct TerrainClamp : public Component<TerrainClamp>
    {
        bool enabled = true;
        bool recenterOrthographic = true;
        bool computeClipRange = true;
    };

    /** Accumulates source bounds for RenderTexture auto-fitting. */
    struct RenderTextureBounds
    {
        bool valid = false;
        SRS srs;
        double referenceLongitude = 0.0;
        double minx = DBL_MAX, miny = DBL_MAX, minz = DBL_MAX;
        double maxx = -DBL_MAX, maxy = -DBL_MAX, maxz = -DBL_MAX;
        double paddingPixels = 2.0;
        double paddingMeters = 0.0;

        void expand(const SRS& inputSRS, const glm::dvec3& input)
        {
            if (!inputSRS.valid())
                return;
            if (!srs.valid())
                srs = inputSRS;

            auto point = GeoPoint(inputSRS, input.x, input.y, input.z).transform(srs);
            if (!point.valid())
                return;

            double x = point.x;
            if (srs.isGeodetic())
            {
                if (!valid)
                    referenceLongitude = x;
                while (x - referenceLongitude > 180.0) x -= 360.0;
                while (x - referenceLongitude < -180.0) x += 360.0;
            }

            minx = std::min(minx, x); miny = std::min(miny, point.y); minz = std::min(minz, point.z);
            maxx = std::max(maxx, x); maxy = std::max(maxy, point.y); maxz = std::max(maxz, point.z);
            valid = true;
        }
    };

    /**
     * Non-destructive change generations for a render-to-texture source set.
     * Bounds changes invalidate auto-fitting; content changes restart baking.
     */
    struct RenderTextureRevision
    {
        std::size_t bounds = 0u;
        std::size_t content = 0u;
    };

    namespace detail
    {
        inline void combineRenderTextureRevision(std::size_t& seed, std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        inline void combineRenderTextureEntity(std::size_t& seed, entt::entity value)
        {
            combineRenderTextureRevision(seed,
                std::hash<std::underlying_type_t<entt::entity>>{}(entt::to_integral(value)));
        }

        template<typename T>
        inline void combineRenderTextureComponent(std::size_t& seed, const T* component)
        {
            if (component)
            {
                // The type token makes component presence/removal observable;
                // the persistent revision makes all dirty() mutations observable.
                combineRenderTextureRevision(seed, entt::type_hash<T>::value());
                combineRenderTextureRevision(seed, component->componentRevision());
            }
        }

        template<typename T>
        inline void combineRenderTextureComponentBoth(
            RenderTextureRevision& revision, const T* component)
        {
            combineRenderTextureComponent(revision.bounds, component);
            combineRenderTextureComponent(revision.content, component);
        }
    }
}
