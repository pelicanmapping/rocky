/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OverlayBakeSystem.h"
#include "OverlayRenderContext.h"
#include "../RTT.h"
#include <rocky/vsg/VSGUtils.h>
#include "ECSTypes.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    struct LegacyOverlayBakeAdapter
    {
        bool ownsRenderTexture = false;
        bool ownsParticipation = false;
    };

    inline glm::uvec2 resolveTextureSize(const Overlay& overlay, unsigned fallback)
    {
        glm::uvec2 size = overlay.textureSize;
        if (size.x == 0u) size.x = fallback;
        if (size.y == 0u) size.y = fallback;
        return size;
    }

    inline glm::uvec2 resolveTextureSize(const RenderTexture& renderTexture, unsigned fallback)
    {
        glm::uvec2 size = renderTexture.textureSize;
        if (size.x == 0u) size.x = fallback;
        if (size.y == 0u) size.y = fallback;
        return size;
    }

    inline std::vector<entt::entity> resolveSources(const RenderTexture& renderTexture, entt::entity owner)
    {
        return renderTexture.sources.empty() ? std::vector<entt::entity>{ owner } : renderTexture.sources;
    }

    struct OverlayBakeViewNode : public vsg::Inherit<vsg::Node, OverlayBakeViewNode>
    {
        vsg::ref_ptr<vsg::View> view;
        vsg::ref_ptr<vsg::Camera> camera;

        SRS worldSRS;
        RenderRequest request;

        vsg::dvec3 eye = { 0.0, 0.0, 1.0 };
        vsg::dvec3 center = { 0.0, 0.0, 0.0 };
        vsg::dvec3 up = { 0.0, 1.0, 0.0 };
        double left = -1.0;
        double right = 1.0;
        double bottom = -1.0;
        double top = 1.0;
        double znear = 0.01;
        double zfar = 1000.0;
        std::uint32_t viewportWidth = 512u;
        std::uint32_t viewportHeight = 512u;

        void traverse(vsg::RecordTraversal& record) const override
        {
            RenderPurpose prevPurpose = RenderPurpose::Main;
            bool hadPrevPurpose = record.getValue(RENDER_PURPOSE_KEY, prevPurpose);

            RenderRequest prevRequest;
            bool hadPrevRequest = record.getValue(RENDER_REQUEST_KEY, prevRequest);

            entt::entity prevTarget = entt::null;
            bool hadPrevTarget = record.getValue(OVERLAY_BAKE_TARGET_KEY, prevTarget);

            SRS prevWorldSRS;
            bool hadPrevWorldSRS = record.getValue("rocky.worldsrs", prevWorldSRS);

            record.setValue(RENDER_PURPOSE_KEY, request.purpose);
            record.setValue(RENDER_REQUEST_KEY, request);
            record.setValue(OVERLAY_BAKE_TARGET_KEY, request.controller);
            record.setValue("rocky.worldsrs", worldSRS);

            if (camera)
            {
                camera->viewMatrix = vsg::LookAt::create(eye, center, up);
                camera->projectionMatrix = vsg::Orthographic::create(left, right, bottom, top, znear, zfar);
                camera->viewportState = vsg::ViewportState::create(VkExtent2D{ viewportWidth, viewportHeight });
            }

            if (view)
                view->accept(record);

            record.setValue(RENDER_PURPOSE_KEY, hadPrevPurpose ? prevPurpose : RenderPurpose::Main);
            record.setValue(RENDER_REQUEST_KEY, hadPrevRequest ? prevRequest : RenderRequest{});
            record.setValue(OVERLAY_BAKE_TARGET_KEY, hadPrevTarget ? prevTarget : entt::null);
            if (hadPrevWorldSRS)
                record.setValue("rocky.worldsrs", prevWorldSRS);
        }

        void traverse(vsg::Visitor& visitor) override
        {
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
        }

        void traverse(vsg::ConstVisitor& visitor) const override
        {
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
        }
    };

    glm::dmat4 makeWorldMatrix(const Transform& xform, const SRS& worldSRS)
    {
        auto pos = xform.position.transform(worldSRS);
        if (!pos.valid())
            return glm::dmat4(1.0);

        if (xform.topocentric)
        {
            return worldSRS.topocentricToWorldMatrix(glm::dvec3(pos.x, pos.y, pos.z)) * xform.localMatrix;
        }
        else
        {
            return glm::translate(glm::dmat4(1.0), glm::dvec3(pos.x, pos.y, pos.z)) * xform.localMatrix;
        }
    }

    Transform* ensureOverlayTransform(
        entt::registry& reg,
        entt::entity e_overlay,
        const std::vector<entt::entity>& sources,
        const std::vector<System*>& participants,
        bool forceRecompute,
        float depthSafetyFactor,
        const SRS& worldSRS,
        const glm::uvec2& textureSize)
    {
        auto* existing = reg.try_get<Transform>(e_overlay);
        bool autoManaged = reg.any_of<AutoOverlayTransform>(e_overlay);
        if (existing && !autoManaged)
            return existing;
        if (existing && autoManaged && !forceRecompute)
            return existing;

        RenderTextureBounds bounds;

        for (auto source : sources)
        {
            if (!reg.valid(source))
                continue;
            for (auto* participant : participants)
                participant->expandRenderTextureBounds(reg, source, bounds, worldSRS);
        }

        if (!bounds.valid || !bounds.srs.valid())
            return existing;

        Transform* xform = existing;
        if (!xform)
        {
            xform = &reg.emplace<Transform>(e_overlay);
            (void)reg.get_or_emplace<AutoOverlayTransform>(e_overlay);
        }
        double centerX = (bounds.minx + bounds.maxx) * 0.5;
        if (bounds.srs.isGeodetic())
            centerX = std::remainder(centerX, 360.0);
        xform->position = GeoPoint(bounds.srs, centerX,
            (bounds.miny + bounds.maxy) * 0.5,
            (bounds.minz + bounds.maxz) * 0.5);

        if (bounds.srs.isGeodetic())
        {
            xform->topocentric = true;

            // Use the largest span across multiple sample lat/lon slices.
            // A single centroid-latitude sample can underestimate width for
            // tall geodetic extents, clipping one side (commonly east).
            auto eastMetersMid = GeoPoint(bounds.srs, bounds.minx, xform->position.y, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, xform->position.y, 0.0))
                .as(Units::METERS);
            auto eastMetersSouth = GeoPoint(bounds.srs, bounds.minx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.miny, 0.0))
                .as(Units::METERS);
            auto eastMetersNorth = GeoPoint(bounds.srs, bounds.minx, bounds.maxy, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto eastMeters = std::max(eastMetersMid, std::max(eastMetersSouth, eastMetersNorth));

            auto northMetersMid = GeoPoint(bounds.srs, xform->position.x, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, xform->position.x, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMetersWest = GeoPoint(bounds.srs, bounds.minx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.minx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMetersEast = GeoPoint(bounds.srs, bounds.maxx, bounds.miny, 0.0)
                .geodesicDistanceTo(GeoPoint(bounds.srs, bounds.maxx, bounds.maxy, 0.0))
                .as(Units::METERS);
            auto northMeters = std::max(northMetersMid, std::max(northMetersWest, northMetersEast));

            eastMeters = std::max(1.0, eastMeters);
            northMeters = std::max(1.0, northMeters);
            eastMeters += 2.0 * eastMeters * bounds.paddingPixels / std::max(1u, textureSize.x);
            northMeters += 2.0 * northMeters * bounds.paddingPixels / std::max(1u, textureSize.y);

            // Choose the minimum practical thickness for the orthographic projector volume.
            // It must cover earth-curvature drop from the tangent frame center to the overlay
            // edges, plus any source Z range. Keeping this tight reduces decal culling load.
            const double verticalRange = std::max(1.0, bounds.maxz - bounds.minz);
            const double verticalHalf = 0.5 * verticalRange;

            const double halfDiagonalMeters = 0.5 * std::sqrt(eastMeters * eastMeters + northMeters * northMeters);
            const double R = bounds.srs.ellipsoid().semiMajorAxis();

            double curvatureDrop = 0.0;
            if (R > 0.0 && halfDiagonalMeters > 0.0)
            {
                double hh = std::min(halfDiagonalMeters, R - 1.0);
                curvatureDrop = R - std::sqrt(std::max(0.0, R * R - hh * hh));
            }

            // Adaptive terrain-relief floor based on footprint size.
            // Keeps small overlays tight and large overlays robust.
            const double diagonalMeters = std::sqrt(eastMeters * eastMeters + northMeters * northMeters);
            const double terrainReliefFloor =
                std::clamp(0.03 * diagonalMeters, 500.0, 4000.0);
            const double margin = 25.0; // meters
            const double requiredHalfDepth =
                curvatureDrop +
                verticalHalf +
                (0.5 * terrainReliefFloor) +
                margin;
            const double minThickness = terrainReliefFloor + (2.0 * margin);
            const double autoThickness = std::max(minThickness, std::max(verticalRange + 1.0, requiredHalfDepth * 2.0));
            const double safety = std::max(0.1, (double)depthSafetyFactor);
            const double thickness = std::max(verticalRange + 1.0, autoThickness * safety);

            xform->localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    eastMeters,
                    northMeters,
                    thickness));
        }
        else
        {
            xform->topocentric = false;
            double width = std::max(1.0, bounds.maxx - bounds.minx);
            double height = std::max(1.0, bounds.maxy - bounds.miny);
            width += 2.0 * width * bounds.paddingPixels / std::max(1u, textureSize.x);
            height += 2.0 * height * bounds.paddingPixels / std::max(1u, textureSize.y);
            const double verticalRange = std::max(1.0, bounds.maxz - bounds.minz);
            const double diagonalMeters = std::sqrt(width * width + height * height);
            const double terrainReliefFloor =
                std::clamp(0.03 * diagonalMeters, 500.0, 4000.0);
            const double safety = std::max(0.1, (double)depthSafetyFactor);
            xform->localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    width,
                    height,
                    std::max(verticalRange + 1.0, terrainReliefFloor * safety)));
        }

        xform->dirty(reg);
        return xform;
    }




    RenderTextureRevision computeRenderTextureRevision(
        entt::registry& reg,
        const std::vector<entt::entity>& sources,
        const std::vector<System*>& participants)
    {
        RenderTextureRevision revision;
        for (auto source : sources)
        {
            detail::combineRenderTextureEntity(revision.bounds, source);
            detail::combineRenderTextureEntity(revision.content, source);
            if (reg.valid(source))
                for (auto* participant : participants)
                    participant->contributeRenderTextureRevision(reg, source, revision);
        }
        return revision;
    }

    std::size_t computeRenderTextureContentRevision(
        entt::registry& reg,
        entt::entity controller,
        const std::vector<entt::entity>& sources,
        std::size_t sourceContentRevision)
    {
        std::size_t revision = sourceContentRevision;

        // A same-entity job moves its camera, geometry, and projector together,
        // so its Transform does not change the pixels being baked. With separate
        // sources, however, source transforms affect content and must invalidate
        // an otherwise static render job.
        const bool sharedFrame = sources.size() == 1u && sources.front() == controller;
        for (auto source : sources)
        {
            if (!sharedFrame)
            {
                if (auto* transform = reg.try_get<Transform>(source))
                {
                    detail::combineRenderTextureRevision(
                        revision, entt::type_hash<Transform>::value());
                    detail::combineRenderTextureRevision(
                        revision, static_cast<std::size_t>(transform->revision));
                }
            }

            if (auto* participation = reg.try_get<RenderParticipation>(source))
                detail::combineRenderTextureRevision(revision, participation->renderTexture ? 1u : 0u);
        }

        return revision;
    }
}

OverlayBakeSystemNode::OverlayBakeSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
        {
            r.on_construct<Overlay>().connect<&OverlayBakeSystemNode::on_construct_Overlay>(*this);
            r.on_update<Overlay>().connect<&OverlayBakeSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Overlay>().connect<&OverlayBakeSystemNode::on_destroy_Overlay>(*this);
            r.on_construct<RenderTexture>().connect<&OverlayBakeSystemNode::on_construct_RenderTexture>(*this);
            r.on_update<RenderTexture>().connect<&OverlayBakeSystemNode::on_update_RenderTexture>(*this);
            r.on_destroy<RenderTexture>().connect<&OverlayBakeSystemNode::on_destroy_RenderTexture>(*this);
            r.on_destroy<OverlayBakeDetail>().connect<&OverlayBakeSystemNode::on_destroy_OverlayBakeDetail>(*this);
        });
}

void OverlayBakeSystemNode::initialize(VSGContext vsgcontext)
{
    for (std::size_t i = 0; i < _sharedCameras.size(); ++i)
    {
        if (!_sharedCameras[i])
        {
            auto extent = VkExtent2D{ textureSize, textureSize };
            _sharedCameras[i] = vsg::Camera::create(
                vsg::Orthographic::create(-1.0, 1.0, -1.0, 1.0, 0.01, 1000.0),
                vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0)),
                vsg::ViewportState::create(extent));
        }

        if (!_sharedViews[i] && bakeScene)
        {
            _sharedViews[i] = vsg::View::create(_sharedCameras[i], bakeScene);

            if (i == 0u)
            {
                auto noDepth = vsg::DepthStencilState::create();
                noDepth->depthTestEnable = VK_FALSE;
                noDepth->depthWriteEnable = VK_FALSE;
                _sharedViews[i]->overridePipelineStates.push_back(noDepth);
            }
        }
    }
}

void OverlayBakeSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<LegacyOverlayBakeAdapter>(e);
    const auto& overlay = r.get<Overlay>(e);

    if (!r.any_of<RenderTexture>(e))
    {
        auto& renderTexture = r.emplace<RenderTexture>(e);
        renderTexture.sources = { e };
        renderTexture.textureSize = overlay.textureSize;
        renderTexture.useDepthBuffer = overlay.useDepthBuffer;
        renderTexture.continuous = overlay.continuousBake;
        adapter.ownsRenderTexture = true;
    }

    if (!r.any_of<RenderParticipation>(e))
    {
        auto& participation = r.emplace<RenderParticipation>(e);
        participation.mainView = false;
        participation.renderTexture = true;
        adapter.ownsParticipation = true;
    }
}

void OverlayBakeSystemNode::on_update_Overlay(entt::registry& r, entt::entity e)
{
    auto* adapter = r.try_get<LegacyOverlayBakeAdapter>(e);
    auto* renderTexture = r.try_get<RenderTexture>(e);
    if (adapter && adapter->ownsRenderTexture && renderTexture)
    {
        const auto& overlay = r.get<Overlay>(e);
        renderTexture->sources = { e };
        renderTexture->textureSize = overlay.textureSize;
        renderTexture->useDepthBuffer = overlay.useDepthBuffer;
        renderTexture->continuous = overlay.continuousBake;
    }
}

void OverlayBakeSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<LegacyOverlayBakeAdapter>(e))
    {
        if (adapter->ownsRenderTexture && r.any_of<RenderTexture>(e))
            r.remove<RenderTexture>(e);
        if (adapter->ownsParticipation && r.any_of<RenderParticipation>(e))
            r.remove<RenderParticipation>(e);
        r.remove<LegacyOverlayBakeAdapter>(e);
    }
}

void OverlayBakeSystemNode::on_construct_RenderTexture(entt::registry& r, entt::entity e)
{
    r.get<RenderTexture>(e).owner = e;
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
}

void OverlayBakeSystemNode::on_update_RenderTexture(entt::registry& r, entt::entity e)
{
    r.get<RenderTexture>(e).owner = e;
}

void OverlayBakeSystemNode::on_destroy_RenderTexture(entt::registry& r, entt::entity e)
{
    if (r.any_of<AutoOverlayTransform>(e))
    {
        r.remove<AutoOverlayTransform>(e);
        if (r.any_of<Transform>(e))
            r.remove<Transform>(e);
    }

    auto* detail = r.try_get<OverlayBakeDetail>(e);
    if (detail && detail->ownsResource && r.any_of<TextureResource>(e))
        r.remove<TextureResource>(e);

    r.remove<OverlayBakeDetail>(e);
}

void OverlayBakeSystemNode::on_destroy_OverlayBakeDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<OverlayBakeDetail>(e);

    if (detail.hostCommandGraph && detail.renderGraph)
    {
        auto& children = detail.hostCommandGraph->children;
        children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
    }

    dispose(detail.renderGraph);
    dispose(detail.texture);
}

bool OverlayBakeSystemNode::createBakeResources(
    VSGContext vsgcontext,
    const glm::uvec2& textureSize,
    bool useDepthBuffer,
    vsg::ref_ptr<vsg::ImageInfo>& outTexture,
    vsg::ref_ptr<vsg::RenderGraph>& outRenderGraph,
    vsg::ref_ptr<vsg::Node>& outViewNode,
    vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph)
{
    if (!bakeScene)
        return false;

    const std::size_t mode = useDepthBuffer ? 1u : 0u;
    if (!_sharedCameras[mode] || !_sharedViews[mode])
        initialize(vsgcontext);

    if (!_sharedCameras[mode] || !_sharedViews[mode])
        return false;

    auto bakeNode = OverlayBakeViewNode::create();
    bakeNode->view = _sharedViews[mode];
    bakeNode->camera = _sharedCameras[mode];
    bakeNode->worldSRS = worldSRS;
    bakeNode->viewportWidth = textureSize.x;
    bakeNode->viewportHeight = textureSize.y;

    auto extent = VkExtent2D{ textureSize.x, textureSize.y };

    auto context = vsg::Context::create(vsgcontext->device());
    auto color = vsg::ImageInfo::create();
    vsg::ref_ptr<vsg::ImageInfo> depth;
    if (useDepthBuffer)
        depth = vsg::ImageInfo::create();
    auto rg = RTT::createOffScreenRenderGraph(*context, extent, color, depth, vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    rg->addChild(bakeNode);

    auto viewer = vsgcontext->viewer();
    if (!viewer)
        return false;

    bool installed = false;
    for (auto& task : viewer->recordAndSubmitTasks)
    {
        for (auto& cg : task->commandGraphs)
        {
            if (!cg || !cg->window)
                continue;

            outHostCommandGraph = cg;
            installed = true;
            break;
        }
        if (installed)
            break;
    }

    if (!installed)
        return false;

    // Compile now, but do not attach until the registry still owns the overlay
    // and its bake camera is valid. This prevents orphan command-graph children.
    vsgcontext->compileRenderGraph(rg, outHostCommandGraph->window);

    outTexture = color;
    outRenderGraph = rg;
    outViewNode = bakeNode;

    vsgcontext->requestFrame();
    return true;
}

bool OverlayBakeSystemNode::updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail, bool recomputeAutoTransform) const
{
    auto* renderTexture = r.try_get<RenderTexture>(e_overlay);
    if (!renderTexture)
        return false;
    auto sources = resolveSources(*renderTexture, e_overlay);

    auto* xform = renderTexture->fitToSources ?
        ensureOverlayTransform(r, e_overlay, sources, renderParticipants, recomputeAutoTransform, depthSafetyFactor, worldSRS, detail.textureSize) :
        r.try_get<Transform>(e_overlay);
    auto* viewNode = dynamic_cast<OverlayBakeViewNode*>(detail.viewNode.get());
    if (!xform || !viewNode || !viewNode->camera || !viewNode->view)
        return false;

    SRS srs = worldSRS.valid() ? worldSRS : (xform->position.srs.isGeodetic() ? xform->position.srs.geocentricSRS() : xform->position.srs);
    if (!srs.valid())
        return false;

    auto world = makeWorldMatrix(*xform, srs);

    glm::dvec3 tx(world[3]);
    glm::dvec3 x(world[0]);
    glm::dvec3 y(world[1]);
    glm::dvec3 z(world[2]);

    double sx = glm::length(x);
    double sy = glm::length(y);
    double sz = glm::length(z);
    if (sx <= 0.0 || sy <= 0.0 || sz <= 0.0)
        return false;

    x /= sx;
    y /= sy;
    z /= sz;

    double depth = std::max(10.0, sz * 4.0);

    viewNode->request.purpose = RenderPurpose::RenderTexture;
    viewNode->request.controller = e_overlay;
    viewNode->request.ignoreSourceTransforms =
        r.any_of<AutoOverlayTransform>(e_overlay) && sources.size() == 1u && sources.front() == e_overlay;
    viewNode->request.sources = std::move(sources);
    viewNode->worldSRS = srs;
    viewNode->eye = to_vsg(tx + z * depth * 0.5);
    viewNode->center = to_vsg(tx);
    viewNode->up = to_vsg(y);
    viewNode->left = -sx * 0.5;
    viewNode->right = sx * 0.5;
    viewNode->bottom = -sy * 0.5;
    viewNode->top = sy * 0.5;
    viewNode->znear = 0.01;
    viewNode->zfar = depth;

    return true;
}

void OverlayBakeSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    bool depthPolicyChanged = std::abs(depthSafetyFactor - _lastDepthSafetyFactor) > 1e-6f;
    _lastDepthSafetyFactor = depthSafetyFactor;

    std::vector<entt::entity> renderJobs;
    std::vector<entt::entity> needsSetup;

    _registry.write([&](entt::registry& r)
        {
            // Overlay remains a public convenience facade. Keep its generated
            // low-level contract synchronized even when callers edit fields by
            // reference instead of using registry.patch().
            r.view<Overlay, LegacyOverlayBakeAdapter, RenderTexture>().each(
                [&](auto e_overlay, auto& overlay, auto& adapter, auto& renderTexture)
                {
                    if (adapter.ownsRenderTexture)
                    {
                        renderTexture.sources = { e_overlay };
                        renderTexture.textureSize = overlay.textureSize;
                        renderTexture.useDepthBuffer = overlay.useDepthBuffer;
                        renderTexture.continuous = overlay.continuousBake;
                    }
                });

            r.view<RenderTexture>().each([&](auto entity, auto&)
                {
                    renderJobs.push_back(entity);
                });
        });

    _registry.write([&](entt::registry& r)
        {
            for (auto renderJob : renderJobs)
            {
                auto& renderTexture = r.get<RenderTexture>(renderJob);
                auto& detail = r.get_or_emplace<OverlayBakeDetail>(renderJob);
                auto requestedSize = resolveTextureSize(renderTexture, textureSize);
                if (depthPolicyChanged)
                    detail.autoTransformDirty = true;
                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph) ||
                    detail.textureSize != requestedSize ||
                    detail.useDepthBuffer != renderTexture.useDepthBuffer)
                    needsSetup.push_back(renderJob);
            }
        });

    struct PendingSetup
    {
        entt::entity e_overlay = entt::null;
        glm::uvec2 textureSize = { 0u, 0u };
        bool useDepthBuffer = false;
        vsg::ref_ptr<vsg::ImageInfo> texture;
        vsg::ref_ptr<vsg::RenderGraph> renderGraph;
        vsg::ref_ptr<vsg::Node> viewNode;
        vsg::ref_ptr<vsg::CommandGraph> hostCommandGraph;
    };

    std::vector<PendingSetup> pending;
    pending.reserve(needsSetup.size());

    for (auto e_overlay : needsSetup)
    {
        glm::uvec2 requestedSize = { textureSize, textureSize };
        bool useDepthBuffer = false;
        _registry.read([&](entt::registry& r)
            {
                if (r.valid(e_overlay) && r.any_of<RenderTexture>(e_overlay))
                {
                    const auto& renderTexture = r.get<RenderTexture>(e_overlay);
                    requestedSize = resolveTextureSize(renderTexture, textureSize);
                    useDepthBuffer = renderTexture.useDepthBuffer;
                }
            });

        PendingSetup p;
        p.e_overlay = e_overlay;
        p.textureSize = requestedSize;
        p.useDepthBuffer = useDepthBuffer;
        if (createBakeResources(vsgcontext, requestedSize, useDepthBuffer, p.texture, p.renderGraph, p.viewNode, p.hostCommandGraph))
            pending.emplace_back(std::move(p));
    }

    _registry.write([&](entt::registry& r)
        {
            for (auto& p : pending)
            {
                if (!r.valid(p.e_overlay) || !r.any_of<RenderTexture>(p.e_overlay) ||
                    resolveTextureSize(r.get<RenderTexture>(p.e_overlay), textureSize) != p.textureSize ||
                    r.get<RenderTexture>(p.e_overlay).useDepthBuffer != p.useDepthBuffer)
                {
                    dispose(p.renderGraph);
                    dispose(p.texture);
                    continue;
                }

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(p.e_overlay);

                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                }

                dispose(detail.renderGraph);
                dispose(detail.texture);

                detail.texture = p.texture;
                detail.renderGraph = p.renderGraph;
                detail.viewNode = p.viewNode;
                detail.hostCommandGraph = p.hostCommandGraph;
                detail.textureSize = p.textureSize;
                detail.useDepthBuffer = p.useDepthBuffer;
                detail.contentRevisionValid = false;
                detail.boundsRevisionValid = false;
                detail.autoTransformDirty = true;
                detail.bakeFramesRemaining = INITIAL_BAKE_FRAMES;
            }

            bool requestAnotherFrame = false;

            for (auto e_overlay : renderJobs)
            {
                if (!r.valid(e_overlay) || !r.any_of<RenderTexture>(e_overlay))
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);
                const auto& renderTexture = r.get<RenderTexture>(e_overlay);
                auto sources = resolveSources(renderTexture, e_overlay);

                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph))
                    continue;

                auto sourceRevision = computeRenderTextureRevision(r, sources, renderParticipants);
                if (!detail.boundsRevisionValid || sourceRevision.bounds != detail.boundsRevision)
                {
                    detail.boundsRevision = sourceRevision.bounds;
                    detail.boundsRevisionValid = true;
                    if (r.any_of<AutoOverlayTransform>(e_overlay))
                        detail.autoTransformDirty = true;
                }

                auto contentRevision = computeRenderTextureContentRevision(
                    r, e_overlay, sources, sourceRevision.content);
                if (!detail.contentRevisionValid || contentRevision != detail.contentRevision)
                {
                    detail.contentRevision = contentRevision;
                    detail.contentRevisionValid = true;
                    detail.bakeFramesRemaining = std::max(detail.bakeFramesRemaining, INITIAL_BAKE_FRAMES);
                }

                if (depthPolicyChanged && r.any_of<AutoOverlayTransform>(e_overlay))
                    detail.bakeFramesRemaining = std::max(detail.bakeFramesRemaining, INITIAL_BAKE_FRAMES);

                if (!updateBakeCamera(r, e_overlay, detail, detail.autoTransformDirty))
                {
                    if (detail.hostCommandGraph && detail.renderGraph)
                    {
                        auto& children = detail.hostCommandGraph->children;
                        children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                    }
                    continue;
                }

                detail.autoTransformDirty = false;

                bool shouldBake = renderTexture.continuous || detail.bakeFramesRemaining > 0u;
                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    auto existing = std::find(children.begin(), children.end(), detail.renderGraph);
                    if (shouldBake && existing == children.end())
                        children.insert(children.begin(), detail.renderGraph);
                    else if (!shouldBake && existing != children.end())
                        children.erase(existing);
                }

                if (renderTexture.continuous)
                {
                    requestAnotherFrame = true;
                }
                else if (detail.bakeFramesRemaining > 0u)
                {
                    --detail.bakeFramesRemaining;
                    requestAnotherFrame = requestAnotherFrame || detail.bakeFramesRemaining > 0u;
                }

                auto* resource = r.try_get<TextureResource>(e_overlay);
                if (!resource)
                {
                    resource = &r.emplace<TextureResource>(e_overlay);
                    detail.ownsResource = true;
                }

                // A pre-existing backend resource is explicit and takes
                // precedence over this automatic producer.
                if (detail.ownsResource)
                {
                    resource->owner = e_overlay;
                    const bool resourceChanged =
                        resource->texture != detail.texture ||
                        detail.styleEntity != e_overlay;
                    if (resourceChanged)
                    {
                        resource->texture = detail.texture;
                        resource->origin = TextureOrigin::UpperLeft;
                        resource->alphaMode = TextureAlphaMode::Premultiplied;
                        if (r.any_of<Overlay>(e_overlay))
                            Overlay::dirty(r, e_overlay);
                        detail.styleEntity = e_overlay;
                    }
                    if (resourceChanged || shouldBake)
                        ++resource->revision;
                }
            }

            if (requestAnotherFrame)
                vsgcontext->requestFrame();
        });

    Inherit::update(vsgcontext);
}
