/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "PolygonSystem.h"
#include "FeatureBuilder.h"
#include "OverlayRenderContext.h"
#include "TextureResource.h"
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Overlay.h>
#include <rocky/ecs/ProjectedTexture.h>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace ROCKY_NAMESPACE::detail
{
    //! Owns disposable triangle resources and source revisions, including while a vector overlay has no triangles.
    struct PolygonCache
    {
        entt::entity geometry = entt::null;
        entt::entity style = entt::null;
        entt::entity sourceGeometry = entt::null;
        entt::entity sourceStyle = entt::null;
        std::uint64_t sourceGeometryRevision = 0u;
        std::uint64_t sourceStyleRevision = 0u;
        bool overlayModeValid = false;
        OverlayMode overlayMode = OverlayMode::Raster;
    };
}

namespace
{
    //! Resolves an explicit resource reference, falling back to the primitive owner.
    template<typename T>
    T* resolveComponent(entt::registry& registry, entt::entity reference, entt::entity owner)
    {
        return registry.try_get<T>(reference != entt::null ? reference : owner);
    }

    //! Ordinary and raster rendering require triangles; vector overlays use the source rings.
    bool needsTriangleCache(entt::registry& registry, entt::entity entity)
    {
        const auto* overlay = registry.try_get<Overlay>(entity);
        return !overlay || resolveOverlayMode(registry, entity, overlay->mode) != OverlayMode::Vector;
    }

    //! Destroys only this polygon's private cache entities under the registry write lock.
    void removeTriangleCache(entt::registry& registry, entt::entity owner, PolygonCache& data)
    {
        registry.remove<PolygonRenderData>(owner);
        const auto geometry = data.geometry;
        const auto style = data.style;
        data.geometry = entt::null;
        data.style = entt::null;

        if (geometry != entt::null && registry.valid(geometry))
            registry.destroy(geometry);
        if (style != entt::null && registry.valid(style))
            registry.destroy(style);
    }

    //! Queues polygons referencing a changed or removed source resource for rebuilding.
    void markPolygonDependents(
        entt::registry& registry,
        entt::entity resource,
        bool geometryResource)
    {
        registry.view<Polygon>().each([&](auto owner, auto& polygon)
        {
            const auto reference = geometryResource ? polygon.geometry : polygon.style;
            if ((reference != entt::null ? reference : owner) == resource)
                polygon.dirty(registry);
        });
    }
}

PolygonSystemNode::PolygonSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
    {
        r.on_construct<Polygon>().connect<&PolygonSystemNode::on_construct_Polygon>(*this);
        r.on_update<Polygon>().connect<&PolygonSystemNode::on_update_Polygon>(*this);
        r.on_destroy<Polygon>().connect<&PolygonSystemNode::on_destroy_Polygon>(*this);
        r.on_destroy<PolygonCache>().connect<&PolygonSystemNode::on_destroy_PolygonCache>(*this);
        r.on_construct<PolygonGeometry>().connect<&PolygonSystemNode::on_construct_PolygonGeometry>(*this);
        r.on_update<PolygonGeometry>().connect<&PolygonSystemNode::on_update_PolygonGeometry>(*this);
        r.on_destroy<PolygonGeometry>().connect<&PolygonSystemNode::on_destroy_PolygonGeometry>(*this);
        r.on_construct<PolygonStyle>().connect<&PolygonSystemNode::on_construct_PolygonStyle>(*this);
        r.on_update<PolygonStyle>().connect<&PolygonSystemNode::on_update_PolygonStyle>(*this);
        r.on_destroy<PolygonStyle>().connect<&PolygonSystemNode::on_destroy_PolygonStyle>(*this);
        r.on_construct<Overlay>().connect<&PolygonSystemNode::on_construct_Overlay>(*this);
        r.on_update<Overlay>().connect<&PolygonSystemNode::on_update_Overlay>(*this);
        r.on_destroy<Overlay>().connect<&PolygonSystemNode::on_destroy_Overlay>(*this);

        auto dirtyOwner = r.create();
        r.emplace<Polygon::Dirty>(dirtyOwner);
        r.emplace<PolygonGeometry::Dirty>(dirtyOwner);
        r.emplace<PolygonStyle::Dirty>(dirtyOwner);

        std::vector<entt::entity> geometries;
        r.view<PolygonGeometry>().each([&](auto entity, auto&) { geometries.emplace_back(entity); });
        for (auto entity : geometries)
            on_construct_PolygonGeometry(r, entity);

        std::vector<entt::entity> styles;
        r.view<PolygonStyle>().each([&](auto entity, auto&) { styles.emplace_back(entity); });
        for (auto entity : styles)
            on_construct_PolygonStyle(r, entity);

        std::vector<entt::entity> polygons;
        r.view<Polygon>().each([&](auto entity, auto&) { polygons.emplace_back(entity); });
        for (auto entity : polygons)
            on_construct_Polygon(r, entity);
    });
}

void PolygonSystemNode::on_construct_Polygon(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    Polygon::dirty(r, e);
}

void PolygonSystemNode::on_update_Polygon(entt::registry& r, entt::entity e)
{
    Polygon::dirty(r, e);
}

void PolygonSystemNode::on_destroy_Polygon(entt::registry& r, entt::entity e)
{
    r.remove<PolygonCache>(e);
}

void PolygonSystemNode::on_destroy_PolygonCache(entt::registry& r, entt::entity e)
{
    removeTriangleCache(r, e, r.get<PolygonCache>(e));
}

void PolygonSystemNode::on_construct_PolygonGeometry(entt::registry& r, entt::entity e)
{
    PolygonGeometry::dirty(r, e);
}

void PolygonSystemNode::on_update_PolygonGeometry(entt::registry& r, entt::entity e)
{
    PolygonGeometry::dirty(r, e);
}

void PolygonSystemNode::on_destroy_PolygonGeometry(entt::registry& r, entt::entity e)
{
    markPolygonDependents(r, e, true);
}

void PolygonSystemNode::on_construct_PolygonStyle(entt::registry& r, entt::entity e)
{
    PolygonStyle::dirty(r, e);
}

void PolygonSystemNode::on_update_PolygonStyle(entt::registry& r, entt::entity e)
{
    PolygonStyle::dirty(r, e);
}

void PolygonSystemNode::on_destroy_PolygonStyle(entt::registry& r, entt::entity e)
{
    markPolygonDependents(r, e, false);
}

void PolygonSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* polygon = r.try_get<Polygon>(e))
        polygon->dirty(r);
}

void PolygonSystemNode::on_update_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* polygon = r.try_get<Polygon>(e))
        polygon->dirty(r);
}

void PolygonSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* polygon = r.try_get<Polygon>(e))
        polygon->dirty(r);
}

RenderTextureSourceStatus PolygonSystemNode::renderTextureSourceStatus(
    entt::registry& registry, entt::entity entity) const
{
    const auto* polygon = registry.try_get<Polygon>(entity);
    if (!polygon)
        return {};

    const auto* geometry = resolveComponent<PolygonGeometry>(
        registry, polygon->geometry, entity);
    return geometry ? RenderTextureSourceStatus{} :
        RenderTextureSourceStatus{
            RenderTextureSourceStatus::State::Waiting,
            "Waiting for polygon geometry",
            true };
}

void PolygonSystemNode::expandRenderTextureBounds(
    entt::registry& registry,
    entt::entity entity,
    RenderTextureBounds& bounds,
    const SRS& worldSRS,
    bool applySourceTransform)
{
    const auto* polygon = registry.try_get<Polygon>(entity);
    if (!polygon)
        return;

    const auto* geometry = resolveComponent<PolygonGeometry>(
        registry, polygon->geometry, entity);
    if (!geometry)
        return;

    for (const auto& part : geometry->polygons)
    {
        for (const auto& point : part.outer)
        {
            expandRenderTextureSourcePoint(
                registry, entity, bounds, geometry->srs, point,
                worldSRS, applySourceTransform);
        }
        for (const auto& hole : part.holes)
        {
            for (const auto& point : hole)
            {
                expandRenderTextureSourcePoint(
                    registry, entity, bounds, geometry->srs, point,
                    worldSRS, applySourceTransform);
            }
        }
    }
}

void PolygonSystemNode::contributeRenderTextureRevision(
    entt::registry& registry,
    entt::entity entity,
    RenderTextureRevision& revision)
{
    const auto* polygon = registry.try_get<Polygon>(entity);
    if (!polygon)
        return;

    combineRenderTextureComponentBoth(revision, polygon);
    combineRenderTextureEntity(revision.bounds, polygon->geometry);
    combineRenderTextureEntity(revision.content, polygon->geometry);
    combineRenderTextureEntity(revision.content, polygon->style);
    combineRenderTextureComponentBoth(
        revision,
        resolveComponent<PolygonGeometry>(registry, polygon->geometry, entity));
    combineRenderTextureComponent(
        revision.content,
        resolveComponent<PolygonStyle>(registry, polygon->style, entity));
    if (const auto* style = resolveComponent<PolygonStyle>(registry, polygon->style, entity))
    {
        combineRenderTextureEntity(revision.content, style->texture);
        if (const auto* texture = registry.try_get<TextureResource>(style->texture))
        {
            combineRenderTextureComponent(revision.content, texture);
            combineRenderTextureRevision(revision.content, texture->revision);
            combineRenderTextureRevision(revision.content, texture->ready);
        }
    }
}

void PolygonSystemNode::update(VSGContext vsgcontext)
{
    _registry.write([&](entt::registry& registry)
    {
        std::unordered_set<entt::entity> changedGeometries;
        std::unordered_set<entt::entity> changedStyles;
        std::unordered_set<entt::entity> changedPolygons;

        // Overlay fields are commonly edited in place and marked dirty rather
        // than patched through EnTT. Poll only polygon overlays for mode
        // transitions so the triangle cache follows Raster/Vector switching reliably.
        registry.view<Polygon, Overlay>().each(
            [&](auto entity, auto& polygon, const auto& overlay)
            {
                auto* data = registry.try_get<PolygonCache>(entity);
                if (data &&
                    (!data->overlayModeValid ||
                        data->overlayMode != resolveOverlayMode(registry, entity, overlay.mode)))
                {
                    // A renderer mode transition changes the derived cache,
                    // not the source Polygon's revision. In particular, do not
                    // invalidate a capacity fallback just by creating its triangles.
                    changedPolygons.insert(entity);
                }
            });

        PolygonGeometry::eachDirty(registry, [&](auto entity)
        {
            changedGeometries.insert(entity);
        });
        PolygonStyle::eachDirty(registry, [&](auto entity)
        {
            changedStyles.insert(entity);
        });

        if (!changedGeometries.empty() || !changedStyles.empty())
        {
            registry.view<Polygon>().each([&](auto owner, auto& polygon)
            {
                const auto geometry = polygon.geometry != entt::null ? polygon.geometry : owner;
                const auto style = polygon.style != entt::null ? polygon.style : owner;
                if (changedGeometries.count(geometry) > 0u ||
                    changedStyles.count(style) > 0u)
                {
                    polygon.dirty(registry);
                }
            });
        }

        Polygon::eachDirty(registry, [&](auto entity)
        {
            changedPolygons.insert(entity);
        });

        for (auto entity : changedPolygons)
        {
            if (!registry.valid(entity))
                continue;

            auto* polygon = registry.try_get<Polygon>(entity);
            if (!polygon)
                continue;

            auto& data = registry.get_or_emplace<PolygonCache>(entity);
            if (const auto* overlay = registry.try_get<Overlay>(entity))
            {
                data.overlayMode = resolveOverlayMode(registry, entity, overlay->mode);
                data.overlayModeValid = true;
            }
            else
            {
                data.overlayModeValid = false;
            }
            if (!needsTriangleCache(registry, entity))
            {
                removeTriangleCache(registry, entity, data);
                continue;
            }

            auto* geometry = resolveComponent<PolygonGeometry>(
                registry, polygon->geometry, entity);
            auto* style = resolveComponent<PolygonStyle>(
                registry, polygon->style, entity);
            const PolygonStyle defaultStyle;
            const auto& resolvedStyle = style ? *style : defaultStyle;

            if (!geometry)
            {
                removeTriangleCache(registry, entity, data);
                continue;
            }

            const auto sourceGeometry = polygon->geometry != entt::null ?
                polygon->geometry : entity;
            const auto sourceStyle = polygon->style != entt::null ?
                polygon->style : entity;
            const auto sourceGeometryRevision = geometry->componentRevision();
            const auto sourceStyleRevision = style ? style->componentRevision() : 0u;

            if (data.geometry == entt::null || !registry.valid(data.geometry))
                data.geometry = registry.create();
            if (data.style == entt::null || !registry.valid(data.style))
                data.style = registry.create();

            const float effectiveDepthOffset = registry.any_of<Overlay>(entity) ?
                0.0f : resolvedStyle.depthOffset;
            const auto* currentMeshStyle =
                registry.try_get<MeshStyle>(data.style);
            const bool resolutionChanged =
                !currentMeshStyle ||
                currentMeshStyle->resolution != resolvedStyle.resolution;
            const bool styleChanged =
                !currentMeshStyle ||
                data.sourceStyle != sourceStyle ||
                data.sourceStyleRevision != sourceStyleRevision ||
                currentMeshStyle->depthOffset != effectiveDepthOffset;
            // Only color inputs baked into vertices require retessellation. Render-state
            // edits (such as wireframe) must reuse geometry even with per-polygon colors.
            const bool colorsChanged =
                !currentMeshStyle ||
                currentMeshStyle->useGeometryColors != resolvedStyle.useGeometryColors ||
                (resolvedStyle.useGeometryColors && currentMeshStyle->color != resolvedStyle.color);
            const bool geometryChanged =
                !registry.any_of<MeshGeometry>(data.geometry) ||
                data.sourceGeometry != sourceGeometry ||
                data.sourceGeometryRevision != sourceGeometryRevision ||
                resolutionChanged ||
                colorsChanged;

            if (geometryChanged)
            {
                MeshGeometry meshGeometry;
                FeatureBuilder builder;
                builder.buildMeshGeometry(*geometry, resolvedStyle, meshGeometry);
                registry.emplace_or_replace<MeshGeometry>(
                    data.geometry, std::move(meshGeometry));
                data.sourceGeometry = sourceGeometry;
                data.sourceGeometryRevision = sourceGeometryRevision;
            }

            if (styleChanged)
            {
                MeshStyle meshStyle;
                meshStyle.color = resolvedStyle.color;
                meshStyle.useGeometryColors = resolvedStyle.useGeometryColors;
                meshStyle.depthOffset = effectiveDepthOffset;
                meshStyle.resolution = resolvedStyle.resolution;
                meshStyle.texture = resolvedStyle.texture;
                meshStyle.wireframe = resolvedStyle.wireframe;
                meshStyle.lighting = resolvedStyle.lighting;
                meshStyle.stipplePattern = resolvedStyle.stipplePattern;
                meshStyle.writeDepth = resolvedStyle.writeDepth;
                meshStyle.drawBackfaces = resolvedStyle.drawBackfaces;
                meshStyle.twoPassAlpha = resolvedStyle.twoPassAlpha;
                meshStyle.transparencyBin = resolvedStyle.transparencyBin;
                registry.emplace_or_replace<MeshStyle>(
                    data.style, std::move(meshStyle));
                data.sourceStyle = sourceStyle;
                data.sourceStyleRevision = sourceStyleRevision;
            }

            // Publish only triangle-backed polygons so vector-only overlays do not
            // enter the shared renderer's per-view draw and picking scans.
            auto& renderData = registry.get_or_emplace<PolygonRenderData>(entity);
            renderData.geometry = data.geometry;
            renderData.style = data.style;
        }
    });

    Inherit::update(vsgcontext);
}
