/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OverlayBakeSystem.h"
#include "OverlayRenderContext.h"
#include "../RTT.h"
#include <rocky/vsg/VSGUtils.h>
#include <rocky/ecs/Decal.h>
#include <rocky/ecs/Mesh.h>
#include <rocky/ecs/Line.h>
#include <rocky/ecs/Point.h>
#include <rocky/ecs/Model.h>
#include <algorithm>
#include <cfloat>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    inline glm::uvec2 resolveTextureSize(const Overlay& overlay, unsigned fallback)
    {
        glm::uvec2 size = overlay.textureSize;
        if (size.x == 0u) size.x = fallback;
        if (size.y == 0u) size.y = fallback;
        return size;
    }

    struct OverlayBakeViewNode : public vsg::Inherit<vsg::Node, OverlayBakeViewNode>
    {
        vsg::ref_ptr<vsg::View> view;
        vsg::ref_ptr<vsg::Camera> camera;

        SRS worldSRS;
        entt::entity target = entt::null;

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
            int prevDomain = RenderDomain_Main;
            bool hadPrevDomain = record.getValue(RENDER_DOMAIN_KEY, prevDomain);

            entt::entity prevTarget = entt::null;
            bool hadPrevTarget = record.getValue(OVERLAY_BAKE_TARGET_KEY, prevTarget);

            SRS prevWorldSRS;
            bool hadPrevWorldSRS = record.getValue("rocky.worldsrs", prevWorldSRS);

            int domain = RenderDomain_OverlayBake;
            record.setValue(RENDER_DOMAIN_KEY, domain);
            record.setValue(OVERLAY_BAKE_TARGET_KEY, target);
            record.setValue("rocky.worldsrs", worldSRS);

            if (camera)
            {
                camera->viewMatrix = vsg::LookAt::create(eye, center, up);
                camera->projectionMatrix = vsg::Orthographic::create(left, right, bottom, top, znear, zfar);
                camera->viewportState = vsg::ViewportState::create(VkExtent2D{ viewportWidth, viewportHeight });
            }

            if (view)
                view->accept(record);

            record.setValue(RENDER_DOMAIN_KEY, hadPrevDomain ? prevDomain : RenderDomain_Main);
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

    Transform* ensureOverlayTransform(entt::registry& reg, entt::entity e_overlay)
    {
        if (auto* existing = reg.try_get<Transform>(e_overlay))
            return existing;

        bool hasBounds = false;
        SRS srs;
        double minx = DBL_MAX, miny = DBL_MAX, minz = DBL_MAX;
        double maxx = -DBL_MAX, maxy = -DBL_MAX, maxz = -DBL_MAX;

        auto expand = [&](const SRS& inSRS, const glm::dvec3& p)
        {
            if (!inSRS.valid())
                return;

            if (!hasBounds)
            {
                srs = inSRS;
                hasBounds = true;
            }
            else if (inSRS != srs)
            {
                return;
            }

            minx = std::min(minx, p.x); miny = std::min(miny, p.y); minz = std::min(minz, p.z);
            maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y); maxz = std::max(maxz, p.z);
        };

        if (reg.all_of<Mesh>(e_overlay))
        {
            auto& mesh = reg.get<Mesh>(e_overlay);
            if (auto* geom = reg.try_get<MeshGeometry>(mesh.geometry))
            {
                for (auto& v : geom->vertices)
                    expand(geom->srs, v);
            }
        }

        if (reg.all_of<Line>(e_overlay))
        {
            auto& line = reg.get<Line>(e_overlay);
            if (auto* geom = reg.try_get<LineGeometry>(line.geometry))
            {
                for (auto& v : geom->points)
                    expand(geom->srs, v);
            }
        }

        if (reg.all_of<Point>(e_overlay))
        {
            auto& point = reg.get<Point>(e_overlay);
            if (auto* geom = reg.try_get<PointGeometry>(point.geometry))
            {
                for (auto& v : geom->points)
                    expand(geom->srs, v);
            }
        }

        if (!hasBounds || !srs.valid())
            return nullptr;

        auto& xform = reg.emplace<Transform>(e_overlay);
        (void)reg.get_or_emplace<AutoOverlayTransform>(e_overlay);
        xform.position = GeoPoint(srs, (minx + maxx) * 0.5, (miny + maxy) * 0.5, (minz + maxz) * 0.5);

        if (srs.isGeodetic())
        {
            xform.topocentric = true;

            auto eastMeters = GeoPoint(srs, minx, xform.position.y, 0.0)
                .geodesicDistanceTo(GeoPoint(srs, maxx, xform.position.y, 0.0))
                .as(Units::METERS);

            auto northMeters = GeoPoint(srs, xform.position.x, miny, 0.0)
                .geodesicDistanceTo(GeoPoint(srs, xform.position.x, maxy, 0.0))
                .as(Units::METERS);

            xform.localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    std::max(1.0, eastMeters),
                    std::max(1.0, northMeters),
                    std::max(1000.0, maxz - minz + 1.0)));
        }
        else
        {
            xform.topocentric = false;
            xform.localMatrix = glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(
                    std::max(1.0, maxx - minx),
                    std::max(1.0, maxy - miny),
                    std::max(1000.0, maxz - minz + 1.0)));
        }

        xform.dirty(reg);
        return &xform;
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
            r.on_destroy<OverlayBakeDetail>().connect<&OverlayBakeSystemNode::on_destroy_OverlayBakeDetail>(*this);
        });
}

void OverlayBakeSystemNode::initialize(VSGContext vsgcontext)
{
    if (!_sharedCamera)
    {
        auto extent = VkExtent2D{ textureSize, textureSize };
        _sharedCamera = vsg::Camera::create(
            vsg::Orthographic::create(-1.0, 1.0, -1.0, 1.0, 0.01, 1000.0),
            vsg::LookAt::create(vsg::dvec3(0.0, 0.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 1.0, 0.0)),
            vsg::ViewportState::create(extent));
    }

    if (!_sharedView && bakeScene)
    {
        _sharedView = vsg::View::create(_sharedCamera, bakeScene);
    }
}

void OverlayBakeSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
}

void OverlayBakeSystemNode::on_update_Overlay(entt::registry& r, entt::entity e)
{
    // nop
}

void OverlayBakeSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (r.any_of<AutoOverlayTransform>(e))
    {
        r.remove<AutoOverlayTransform>(e);
        if (r.any_of<Transform>(e))
            r.remove<Transform>(e);
    }

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
    vsg::ref_ptr<vsg::ImageInfo>& outTexture,
    vsg::ref_ptr<vsg::RenderGraph>& outRenderGraph,
    vsg::ref_ptr<vsg::Node>& outViewNode,
    vsg::ref_ptr<vsg::CommandGraph>& outHostCommandGraph)
{
    if (!bakeScene)
        return false;

    if (!_sharedCamera || !_sharedView)
        initialize(vsgcontext);

    if (!_sharedCamera || !_sharedView)
        return false;

    auto bakeNode = OverlayBakeViewNode::create();
    bakeNode->view = _sharedView;
    bakeNode->camera = _sharedCamera;
    bakeNode->worldSRS = worldSRS;
    bakeNode->viewportWidth = textureSize.x;
    bakeNode->viewportHeight = textureSize.y;

    auto extent = VkExtent2D{ textureSize.x, textureSize.y };

    auto context = vsg::Context::create(vsgcontext->device());
    auto color = vsg::ImageInfo::create();
    auto depth = vsg::ImageInfo::create();
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

            cg->children.insert(cg->children.begin(), rg);
            vsgcontext->compileRenderGraph(rg, cg->window);
                outHostCommandGraph = cg;
            installed = true;
            break;
        }
        if (installed)
            break;
    }

    if (!installed)
        return false;

    outTexture = color;
    outRenderGraph = rg;
    outViewNode = bakeNode;

    vsgcontext->requestFrame();
    return true;
}

bool OverlayBakeSystemNode::updateBakeCamera(entt::registry& r, entt::entity e_overlay, OverlayBakeDetail& detail) const
{
    auto* xform = ensureOverlayTransform(r, e_overlay);
    auto* viewNode = dynamic_cast<OverlayBakeViewNode*>(detail.viewNode.get());
    if (!xform || !viewNode || !_sharedCamera || !_sharedView)
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

    viewNode->target = e_overlay;
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

    std::vector<entt::entity> overlays;
    std::vector<entt::entity> needsSetup;

    _registry.read([&](entt::registry& r)
        {
            auto view = r.view<Overlay>();
            view.each([&](auto e_overlay, auto&)
                {
                    if (!r.any_of<Decal>(e_overlay))
                        overlays.push_back(e_overlay);
                });
        });

    _registry.write([&](entt::registry& r)
        {
            for (auto e_overlay : overlays)
            {
                auto& overlay = r.get<Overlay>(e_overlay);
                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);
                auto requestedSize = resolveTextureSize(overlay, textureSize);
                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph) || detail.textureSize != requestedSize)
                    needsSetup.push_back(e_overlay);
            }
        });

    struct PendingSetup
    {
        entt::entity e_overlay = entt::null;
        glm::uvec2 textureSize = { 0u, 0u };
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
        _registry.read([&](entt::registry& r)
            {
                if (r.valid(e_overlay) && r.any_of<Overlay>(e_overlay))
                    requestedSize = resolveTextureSize(r.get<Overlay>(e_overlay), textureSize);
            });

        PendingSetup p;
        p.e_overlay = e_overlay;
        p.textureSize = requestedSize;
        if (createBakeResources(vsgcontext, requestedSize, p.texture, p.renderGraph, p.viewNode, p.hostCommandGraph))
            pending.emplace_back(std::move(p));
    }

    _registry.write([&](entt::registry& r)
        {
            for (auto& p : pending)
            {
                if (!r.valid(p.e_overlay) || !r.any_of<Overlay>(p.e_overlay))
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(p.e_overlay);

                if (detail.hostCommandGraph && detail.renderGraph)
                {
                    auto& children = detail.hostCommandGraph->children;
                    children.erase(std::remove(children.begin(), children.end(), detail.renderGraph), children.end());
                }

                detail.texture = p.texture;
                detail.renderGraph = p.renderGraph;
                detail.viewNode = p.viewNode;
                detail.hostCommandGraph = p.hostCommandGraph;
                detail.textureSize = p.textureSize;
            }

            for (auto e_overlay : overlays)
            {
                if (!r.valid(e_overlay) || !r.any_of<Overlay>(e_overlay))
                    continue;

                auto& detail = r.get_or_emplace<OverlayBakeDetail>(e_overlay);

                if (!(detail.renderGraph && detail.texture && detail.viewNode && detail.hostCommandGraph))
                    continue;

                if (!updateBakeCamera(r, e_overlay, detail))
                    continue;

                auto& bakeTexture = r.get_or_emplace<OverlayBakeTexture>(e_overlay);
                if (bakeTexture.texture != detail.texture || detail.styleEntity != e_overlay)
                {
                    bakeTexture.texture = detail.texture;
                    Overlay::dirty(r, e_overlay);
                    detail.styleEntity = e_overlay;
                }
            }
        });

    Inherit::update(vsgcontext);
}
