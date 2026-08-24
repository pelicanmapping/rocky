/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OverlayBakeSystem.h"
#include "OverlayRenderContext.h"
#include "RenderTextureParticipant.h"
#include "../RTT.h"
#include "../ViewDependentState.h"
#include <rocky/vsg/VSGUtils.h>
#include "ECSTypes.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <exception>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    struct LegacyOverlayBakeAdapter
    {
        bool ownsRenderTexture = false;
        bool ownsParticipation = false;
    };

    inline glm::uvec2 resolveTextureSize(const RenderTexture& renderTexture, unsigned fallback)
    {
        glm::uvec2 size = renderTexture.textureSize;
        if (size.x == 0u) size.x = std::max(1u, fallback);
        if (size.y == 0u) size.y = std::max(1u, fallback);
        return size;
    }

    inline std::vector<entt::entity> resolveSources(const RenderTexture& renderTexture, entt::entity owner)
    {
        return renderTexture.sources.empty() ? std::vector<entt::entity>{ owner } : renderTexture.sources;
    }

    inline bool participatesInRenderTexture(entt::registry& registry, entt::entity source)
    {
        if (!registry.valid(source) || !registry.any_of<ActiveState>(source))
            return false;
        auto* participation = registry.try_get<RenderParticipation>(source);
        return !participation || participation->renderTexture;
    }

    RenderTextureSourceStatus getRenderTextureSourceStatus(
        entt::registry& registry,
        const std::vector<entt::entity>& sources,
        const std::vector<RenderTextureParticipant*>& participants)
    {
        bool hasSource = false;
        for (auto source : sources)
        {
            if (!participatesInRenderTexture(registry, source))
                continue;
            hasSource = true;
            for (auto* participant : participants)
            {
                auto status = participant->renderTextureSourceStatus(registry, source);
                if (status.state != RenderTextureSourceStatus::State::Ready)
                    return status;
            }
        }
        return hasSource ? RenderTextureSourceStatus{} :
            RenderTextureSourceStatus{ RenderTextureSourceStatus::State::Waiting, "No active render-to-texture source" };
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
            else
                record.setValue("rocky.worldsrs", SRS{});
        }

        void traverse(vsg::Visitor& visitor) override
        {
            SRS previous;
            bool hadPrevious = visitor.getValue("rocky.worldsrs", previous);
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
            visitor.setValue("rocky.worldsrs", hadPrevious ? previous : SRS{});
        }

        void traverse(vsg::ConstVisitor& visitor) const override
        {
            SRS previous;
            bool hadPrevious = visitor.getValue("rocky.worldsrs", previous);
            visitor.setValue("rocky.worldsrs", worldSRS);
            if (view)
                view->accept(visitor);
            visitor.setValue("rocky.worldsrs", hadPrevious ? previous : SRS{});
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
        const std::vector<RenderTextureParticipant*>& participants,
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

        const bool sharedFrame = sources.size() == 1u && sources.front() == e_overlay;
        for (auto source : sources)
        {
            if (!participatesInRenderTexture(reg, source))
                continue;
            for (auto* participant : participants)
                participant->expandRenderTextureBounds(reg, source, bounds, worldSRS, !sharedFrame);
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
        entt::entity controller,
        const std::vector<entt::entity>& sources,
        const std::vector<RenderTextureParticipant*>& participants)
    {
        RenderTextureRevision revision;
        const bool sharedFrame = sources.size() == 1u && sources.front() == controller;
        for (auto source : sources)
        {
            detail::combineRenderTextureEntity(revision.bounds, source);
            detail::combineRenderTextureEntity(revision.content, source);
            if (!reg.valid(source))
                continue;

            for (auto* participant : participants)
                participant->contributeRenderTextureRevision(reg, source, revision);

            const bool active = reg.any_of<ActiveState>(source);
            detail::combineRenderTextureRevision(revision.bounds, active ? 1u : 0u);
            detail::combineRenderTextureRevision(revision.content, active ? 1u : 0u);

            const bool participating = participatesInRenderTexture(reg, source);
            detail::combineRenderTextureRevision(revision.bounds, participating ? 1u : 0u);
            detail::combineRenderTextureRevision(revision.content, participating ? 1u : 0u);

            if (!sharedFrame)
            {
                if (auto* transform = reg.try_get<Transform>(source))
                {
                    detail::combineRenderTextureComponentBoth(revision, transform);
                    detail::combineRenderTextureRevision(revision.bounds, static_cast<std::size_t>(transform->revision));
                    detail::combineRenderTextureRevision(revision.content, static_cast<std::size_t>(transform->revision));
                }
            }

            if (auto* visibility = reg.try_get<Visibility>(source))
            {
                detail::combineRenderTextureRevision(revision.content, entt::type_hash<Visibility>::value());
                for (bool visible : visibility->visible)
                    detail::combineRenderTextureRevision(revision.content, visible ? 1u : 0u);
            }
        }
        return revision;
    }
}

OverlayBakeSystemNode::OverlayBakeSystemNode(Registry& registry) :
    Inherit(registry)
{
    bakeScene = vsg::Group::create();

    _registry.write([&](entt::registry& r)
        {
            r.on_construct<Overlay>().connect<&OverlayBakeSystemNode::on_construct_Overlay>(*this);
            r.on_update<Overlay>().connect<&OverlayBakeSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Overlay>().connect<&OverlayBakeSystemNode::on_destroy_Overlay>(*this);
            r.on_construct<RenderTexture>().connect<&OverlayBakeSystemNode::on_construct_RenderTexture>(*this);
            r.on_update<RenderTexture>().connect<&OverlayBakeSystemNode::on_update_RenderTexture>(*this);
            r.on_destroy<RenderTexture>().connect<&OverlayBakeSystemNode::on_destroy_RenderTexture>(*this);
            r.on_destroy<OverlayBakeDetail>().connect<&OverlayBakeSystemNode::on_destroy_OverlayBakeDetail>(*this);

            std::vector<entt::entity> overlays;
            r.view<Overlay>().each([&](auto entity, auto&) { overlays.push_back(entity); });
            for (auto entity : overlays)
                on_construct_Overlay(r, entity);

            std::vector<entt::entity> renderTextures;
            r.view<RenderTexture>().each([&](auto entity, auto&) { renderTextures.push_back(entity); });
            for (auto entity : renderTextures)
                on_construct_RenderTexture(r, entity);
        });

}

void OverlayBakeSystemNode::initialize(VSGContext vsgcontext)
{
    refreshRenderParticipants();

    for (std::size_t i = 0; i < _sharedCameras.size(); ++i)
    {
        if (!_sharedCameras[i])
        {
            auto extent = VkExtent2D{
                std::max(1u, textureSize),
                std::max(1u, textureSize) };
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

        // Rocky graphics pipelines use the extended view-dependent descriptor set.
        // Views created directly by VSG get its stock descriptor set instead, which
        // is not layout-compatible with those pipelines. Keep this VDS private to
        // the bake view: registering it as an application view would make the
        // frustum/decal compute systems consume it before its render graph compiles.
        if (_sharedViews[i] && !viewDependentState(_sharedViews[i]))
        {
            auto vds = ViewDependentStateEx::create(_sharedViews[i], vsgcontext->device());
            _sharedViews[i]->viewDependentState = vds;
        }
    }
}

void OverlayBakeSystemNode::refreshRenderParticipants()
{
    if (!renderSourceSystems)
        return;

    std::vector<RenderTextureParticipant*> participants;
    for (auto* system : renderSourceSystems->systems)
    {
        if (auto* participant = system->renderTextureParticipant())
            participants.emplace_back(participant);
    }

    std::stable_sort(participants.begin(), participants.end(), [](auto* lhs, auto* rhs)
        {
            return lhs->renderTextureOrder() < rhs->renderTextureOrder();
        });

    if (participants == _renderParticipants)
        return;

    _renderParticipants = std::move(participants);
    bakeScene->children.clear();
    for (auto* participant : _renderParticipants)
    {
        if (auto* node = participant->renderTextureNode())
            bakeScene->addChild(vsg::ref_ptr<vsg::Node>(node));
    }

    for (auto& view : _sharedViews)
        requestCompile(view);
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
    (void)r.get_or_emplace<RenderTextureStatus>(e);
}

void OverlayBakeSystemNode::on_update_RenderTexture(entt::registry& r, entt::entity e)
{
    r.get<RenderTexture>(e).owner = e;
    if (auto* detail = r.try_get<OverlayBakeDetail>(e))
    {
        // An explicit component update is also the caller's way to retry a
        // resource allocation that previously failed for this configuration.
        detail->setupFailed = false;
        detail->setupFailure.clear();
    }
}

void OverlayBakeSystemNode::on_destroy_RenderTexture(entt::registry& r, entt::entity e)
{
    if (r.any_of<AutoOverlayTransform>(e))
    {
        r.remove<AutoOverlayTransform>(e);
        if (r.any_of<Transform>(e))
            r.remove<Transform>(e);
    }

    auto* resource = r.try_get<TextureResource>(e);
    if (resource && resource->producer == TextureResourceProducer::RenderTexture)
        r.remove<TextureResource>(e);

    r.remove<OverlayBakeDetail>(e);
    r.remove<RenderTextureStatus>(e);
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
    vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph,
    std::string& outFailure)
{
    outTexture = {};
    outRenderGraph = {};
    outViewNode = {};
    outHostCommandGraph = {};
    outFailure.clear();

    try
    {
    if (!bakeScene)
    {
        outFailure = "RenderTexture bake scene is unavailable";
        return false;
    }

    auto device = vsgcontext ? vsgcontext->device() : nullptr;
    auto physicalDevice = device ? device->getPhysicalDevice() : nullptr;
    if (!physicalDevice)
    {
        outFailure = "RenderTexture Vulkan device is unavailable";
        return false;
    }

    const auto& limits = physicalDevice->getProperties().limits;
    const auto maxWidth = std::min(limits.maxImageDimension2D, limits.maxFramebufferWidth);
    const auto maxHeight = std::min(limits.maxImageDimension2D, limits.maxFramebufferHeight);
    if (textureSize.x == 0u || textureSize.y == 0u ||
        textureSize.x > maxWidth || textureSize.y > maxHeight)
    {
        outFailure =
            "Requested RenderTexture size " + std::to_string(textureSize.x) + "x" +
            std::to_string(textureSize.y) + " exceeds the device limit of " +
            std::to_string(maxWidth) + "x" + std::to_string(maxHeight);
        return false;
    }

    const std::size_t mode = useDepthBuffer ? 1u : 0u;
    if (!_sharedCameras[mode] || !_sharedViews[mode])
        initialize(vsgcontext);

    if (!_sharedCameras[mode] || !_sharedViews[mode])
    {
        outFailure = "RenderTexture bake view could not be initialized";
        return false;
    }

    auto viewer = vsgcontext->viewer();
    if (!viewer)
    {
        outFailure = "RenderTexture viewer is unavailable";
        return false;
    }

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

    // A viewer can briefly have no host graph while it is being realized.
    // Report an empty failure string so the caller treats this as retryable.
    if (!installed)
        return false;

    auto bakeNode = OverlayBakeViewNode::create();
    bakeNode->view = _sharedViews[mode];
    bakeNode->camera = _sharedCameras[mode];
    bakeNode->worldSRS = worldSRS;
    bakeNode->viewportWidth = textureSize.x;
    bakeNode->viewportHeight = textureSize.y;

    auto extent = VkExtent2D{ textureSize.x, textureSize.y };
    const bool registerSharedView = !_sharedRenderPasses[mode];

    // A Context owns the Vulkan memory pools used while creating images.
    // Reusing one context lets independent overlay targets share allocation
    // blocks instead of reserving a large block for every small tile texture.
    if (!_resourceContext)
        _resourceContext = vsg::Context::create(vsgcontext->device());

    auto& context = *_resourceContext;
    auto color = vsg::ImageInfo::create();
    vsg::ref_ptr<vsg::ImageInfo> depth;
    if (useDepthBuffer)
        depth = vsg::ImageInfo::create();
    auto rg = RTT::createOffScreenRenderGraph(
        context,
        extent,
        color,
        depth,
        vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f),
        _sharedRenderPasses[mode]);

    rg->addChild(bakeNode);

    // Compile the shared View/pipelines only for the first compatible target.
    // Substitute a pipeline-only scene while compiling so registry-backed
    // systems do not collect every live entity merely to prepare a new render
    // pass. The runtime scene is restored before the graph can be recorded.
    if (registerSharedView)
    {
        auto compileScene = vsg::Group::create();
        for (auto* participant : _renderParticipants)
            if (auto node = participant->renderTextureCompileNode())
                compileScene->addChild(node);

        auto& viewChildren = _sharedViews[mode]->children;
        auto runtimeChildren = std::move(viewChildren);
        viewChildren.clear();
        viewChildren.emplace_back(compileScene);
        try
        {
            vsgcontext->compileRenderGraph(rg, outHostCommandGraph->window);
        }
        catch (...)
        {
            viewChildren = std::move(runtimeChildren);
            throw;
        }
        viewChildren = std::move(runtimeChildren);
    }

    if (!_sharedRenderPasses[mode])
        _sharedRenderPasses[mode] = vsg::ref_ptr<vsg::RenderPass>(rg->getRenderPass());

    outTexture = color;
    outRenderGraph = rg;
    outViewNode = bakeNode;

    vsgcontext->requestFrame();
    return true;
    }
    catch (const vsg::Exception& e)
    {
        outFailure = e.message.empty() ?
            "Vulkan failed while allocating RenderTexture resources" : e.message;
    }
    catch (const std::exception& e)
    {
        outFailure = std::string("Failed to allocate RenderTexture resources: ") + e.what();
    }
    catch (...)
    {
        outFailure = "Failed to allocate RenderTexture resources";
    }

    outTexture = {};
    outRenderGraph = {};
    outViewNode = {};
    outHostCommandGraph = {};
    return false;
}

bool OverlayBakeSystemNode::updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail, bool recomputeAutoTransform) const
{
    auto* renderTexture = r.try_get<RenderTexture>(e_overlay);
    if (!renderTexture)
        return false;
    auto sources = resolveSources(*renderTexture, e_overlay);

    auto* xform = renderTexture->fitToSources ?
        ensureOverlayTransform(r, e_overlay, sources, _renderParticipants, recomputeAutoTransform, depthSafetyFactor, worldSRS, detail.textureSize) :
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
    refreshRenderParticipants();

    if (status.failed())
        return;

    bool depthPolicyChanged = std::abs(depthSafetyFactor - _lastDepthSafetyFactor) > 1e-6f;
    _lastDepthSafetyFactor = depthSafetyFactor;
    bool worldSRSChanged = worldSRS != _lastWorldSRS;
    _lastWorldSRS = worldSRS;

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
                    auto& jobStatus = r.get_or_emplace<RenderTextureStatus>(entity);
                    if (r.any_of<ImageTexture>(entity))
                    {
                        jobStatus.state = RenderTextureState::Failed;
                        jobStatus.message = "ImageTexture and RenderTexture cannot produce the same TextureResource";
                        r.remove<OverlayBakeDetail>(entity);
                        if (auto* resource = r.try_get<TextureResource>(entity);
                            resource && resource->producer == TextureResourceProducer::RenderTexture)
                            r.remove<TextureResource>(entity);
                        return;
                    }

                    auto* resource = r.try_get<TextureResource>(entity);
                    if (resource && resource->producer != TextureResourceProducer::RenderTexture)
                    {
                        jobStatus.state = RenderTextureState::Failed;
                        jobStatus.message = "TextureResource is owned by another producer";
                        r.remove<OverlayBakeDetail>(entity);
                        return;
                    }
                    if (!resource)
                    {
                        resource = &r.emplace<TextureResource>(entity);
                        resource->producer = TextureResourceProducer::RenderTexture;
                        resource->ready = false;
                    }

                    resource->owner = entity;
                    jobStatus.state = RenderTextureState::WaitingForResources;
                    jobStatus.message.clear();
                    renderJobs.push_back(entity);
                });

            r.view<RenderParticipation>().each([&](auto entity, auto& participation)
                {
                    participation.owner = entity;
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
                if (worldSRSChanged)
                {
                    detail.autoTransformDirty = true;
                    detail.phase = BakePhase::Priming;
                    detail.generationPending = true;
                }
                if (detail.fitToSources != renderTexture.fitToSources)
                {
                    detail.fitToSources = renderTexture.fitToSources;
                    detail.autoTransformDirty = true;
                    detail.phase = BakePhase::Priming;
                    detail.generationPending = true;
                }
                const bool setupRequired =
                    !(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph) ||
                    detail.textureSize != requestedSize ||
                    detail.useDepthBuffer != renderTexture.useDepthBuffer;
                if (setupRequired)
                {
                    auto& resource = r.get<TextureResource>(renderJob);
                    if (resource.ready)
                    {
                        resource.ready = false;
                        ++resource.revision;
                    }

                    auto& jobStatus = r.get<RenderTextureStatus>(renderJob);
                    if (detail.setupFailed &&
                        detail.failedTextureSize == requestedSize &&
                        detail.failedUseDepthBuffer == renderTexture.useDepthBuffer)
                    {
                        jobStatus.state = RenderTextureState::Failed;
                        jobStatus.message = detail.setupFailure;
                        continue;
                    }

                    detail.setupFailed = false;
                    detail.setupFailure.clear();
                    needsSetup.push_back(renderJob);
                }
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
        std::string failure;
    };

    std::vector<PendingSetup> pending;
    std::vector<PendingSetup> failed;
    bool retryResourceSetup = false;
    const auto setupLimit = maxResourceSetupsPerFrame == 0u ?
        needsSetup.size() : std::min(needsSetup.size(), static_cast<std::size_t>(maxResourceSetupsPerFrame));
    pending.reserve(setupLimit);

    for (std::size_t setupIndex = 0u; setupIndex < setupLimit; ++setupIndex)
    {
        auto e_overlay = needsSetup[setupIndex];
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
        if (createBakeResources(
            vsgcontext,
            requestedSize,
            useDepthBuffer,
            p.texture,
            p.renderGraph,
            p.viewNode,
            p.hostCommandGraph,
            p.failure))
        {
            pending.emplace_back(std::move(p));
        }
        else if (!p.failure.empty())
        {
            failed.emplace_back(std::move(p));
        }
        else
        {
            // The viewer can briefly have no host command graph while it is
            // being realized. Keep this as a retryable resource wait.
            retryResourceSetup = true;
        }
    }

    _registry.write([&](entt::registry& r)
        {
            for (auto& p : failed)
            {
                if (!r.valid(p.e_overlay) || !r.any_of<RenderTexture>(p.e_overlay) ||
                    resolveTextureSize(r.get<RenderTexture>(p.e_overlay), textureSize) != p.textureSize ||
                    r.get<RenderTexture>(p.e_overlay).useDepthBuffer != p.useDepthBuffer)
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(p.e_overlay);
                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                }

                dispose(detail.renderGraph);
                dispose(detail.texture);
                detail.renderGraph = {};
                detail.texture = {};
                detail.viewNode = {};
                detail.hostCommandGraph = {};
                detail.textureSize = p.textureSize;
                detail.useDepthBuffer = p.useDepthBuffer;
                detail.setupFailed = true;
                detail.failedTextureSize = p.textureSize;
                detail.failedUseDepthBuffer = p.useDepthBuffer;
                detail.setupFailure = p.failure;
                detail.published = false;
                detail.generationPending = true;

                auto& resource = r.get<TextureResource>(p.e_overlay);
                const bool resourceChanged = resource.ready || resource.texture;
                resource.ready = false;
                resource.texture = {};
                if (resourceChanged)
                    ++resource.revision;

                auto& jobStatus = r.get<RenderTextureStatus>(p.e_overlay);
                jobStatus.state = RenderTextureState::Failed;
                jobStatus.message = p.failure;
            }

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
                detail.fitToSources = r.get<RenderTexture>(p.e_overlay).fitToSources;
                detail.published = false;
                detail.generationPending = true;
                detail.setupFailed = false;
                detail.setupFailure.clear();
                detail.phase = BakePhase::Priming;

                auto& resource = r.get<TextureResource>(p.e_overlay);
                resource.texture = detail.texture;
                resource.origin = TextureOrigin::UpperLeft;
                resource.alphaMode = TextureAlphaMode::Premultiplied;
                resource.ready = false;
                ++resource.revision;
            }

            bool requestAnotherFrame = setupLimit < needsSetup.size() || retryResourceSetup;

            for (auto e_overlay : renderJobs)
            {
                if (!r.valid(e_overlay) || !r.any_of<RenderTexture>(e_overlay))
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);
                const auto& renderTexture = r.get<RenderTexture>(e_overlay);
                auto& jobStatus = r.get_or_emplace<RenderTextureStatus>(e_overlay);
                auto sources = resolveSources(renderTexture, e_overlay);

                // Do not keep rendering or republish an old target while a
                // changed texture configuration is queued for setup.
                if (detail.setupFailed ||
                    detail.textureSize != resolveTextureSize(renderTexture, textureSize) ||
                    detail.useDepthBuffer != renderTexture.useDepthBuffer)
                    continue;

                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph))
                    continue;

                auto& resource = r.get<TextureResource>(e_overlay);

                auto detachRenderGraph = [&]()
                    {
                        auto& children = detail.hostCommandGraph->children;
                        auto existing = std::find(children.begin(), children.end(), detail.renderGraph);
                        if (existing != children.end())
                            children.erase(existing);
                    };

                auto invalidatePublishedTexture = [&]()
                    {
                        if (resource.ready)
                        {
                            resource.ready = false;
                            ++resource.revision;
                        }
                        detail.published = false;
                        detail.generationPending = true;
                    };

                auto sourceStatus = getRenderTextureSourceStatus(r, sources, _renderParticipants);
                if (sourceStatus.state != RenderTextureSourceStatus::State::Ready)
                {
                    detail.phase = BakePhase::WaitingForSources;
                    jobStatus.state = sourceStatus.state == RenderTextureSourceStatus::State::Failed ?
                        RenderTextureState::Failed : RenderTextureState::WaitingForSources;
                    jobStatus.message = sourceStatus.message;
                    detachRenderGraph();
                    if (sourceStatus.state == RenderTextureSourceStatus::State::Failed ||
                        !sourceStatus.retry)
                    {
                        invalidatePublishedTexture();
                    }
                    requestAnotherFrame = requestAnotherFrame ||
                        (sourceStatus.state == RenderTextureSourceStatus::State::Waiting && sourceStatus.retry);
                    continue;
                }

                auto sourceRevision = computeRenderTextureRevision(r, e_overlay, sources, _renderParticipants);
                if (!detail.boundsRevisionValid || sourceRevision.bounds != detail.boundsRevision)
                {
                    detail.boundsRevision = sourceRevision.bounds;
                    detail.boundsRevisionValid = true;
                    if (r.any_of<AutoOverlayTransform>(e_overlay))
                    {
                        detail.autoTransformDirty = true;
                        detail.phase = BakePhase::Priming;
                        detail.generationPending = true;
                    }
                }

                auto contentRevision = sourceRevision.content;
                if (!detail.contentRevisionValid || contentRevision != detail.contentRevision)
                {
                    detail.contentRevision = contentRevision;
                    detail.contentRevisionValid = true;
                    detail.phase = BakePhase::Priming;
                    detail.generationPending = true;
                }

                if (depthPolicyChanged && r.any_of<AutoOverlayTransform>(e_overlay))
                {
                    detail.phase = BakePhase::Priming;
                    detail.generationPending = true;
                }

                if (!updateBakeCamera(r, e_overlay, detail, detail.autoTransformDirty))
                {
                    detail.phase = BakePhase::WaitingForSources;
                    jobStatus.state = RenderTextureState::WaitingForSources;
                    jobStatus.message = "Waiting for valid source bounds and projector transform";
                    detachRenderGraph();
                    invalidatePublishedTexture();
                    continue;
                }

                detail.autoTransformDirty = false;
                if (detail.phase == BakePhase::WaitingForSources)
                    detail.phase = BakePhase::Priming;

                const bool shouldBake = renderTexture.continuous || detail.phase != BakePhase::Ready;
                {
                    auto& children = detail.hostCommandGraph->children;
                    auto existing = std::find(children.begin(), children.end(), detail.renderGraph);
                    if (shouldBake && existing == children.end())
                        children.insert(children.begin(), detail.renderGraph);
                    else if (!shouldBake && existing != children.end())
                        children.erase(existing);
                }

                jobStatus.message.clear();
                if (detail.phase == BakePhase::Priming)
                {
                    jobStatus.state = RenderTextureState::Priming;
                    detail.phase = BakePhase::Baking;
                    requestAnotherFrame = true;
                }
                else if (detail.phase == BakePhase::Baking)
                {
                    jobStatus.state = RenderTextureState::Baking;
                    detail.phase = BakePhase::Ready;
                    requestAnotherFrame = true;
                }
                else
                {
                    jobStatus.state = RenderTextureState::Ready;
                    if (detail.generationPending)
                    {
                        detail.generationPending = false;
                        detail.published = true;
                        resource.ready = true;
                        ++resource.revision;
                        ++jobStatus.generation;
                        if (r.any_of<Overlay>(e_overlay))
                            Overlay::dirty(r, e_overlay);
                    }
                    if (renderTexture.continuous)
                    {
                        ++resource.revision;
                        requestAnotherFrame = true;
                    }
                }
            }

            if (requestAnotherFrame)
                vsgcontext->requestFrame();
        });

    Inherit::update(vsgcontext);
}
