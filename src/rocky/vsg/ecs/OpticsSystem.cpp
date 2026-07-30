/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "OpticsSystem.h"
#include "../ShaderDefines.h"
#include <rocky/vsg/VSGUtils.h>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

namespace
{
    // nop
}


void OpticsSystemNode::on_construct_Optics(entt::registry& r, entt::entity e)
{
    r.emplace<OpticsDetail>(e);
}

void OpticsSystemNode::on_destroy_Optics(entt::registry& r, entt::entity e)
{
    r.remove<OpticsDetail>(e);
}

OpticsSystemNode::OpticsSystemNode(Registry& registry) :
    Inherit(registry)
{
    registry.write([&](entt::registry& r)
        {
            // install the ENTT callbacks for managing internal data:
            r.on_construct<Optics>().connect<&OpticsSystemNode::on_construct_Optics>(*this);
            r.on_destroy<Optics>().connect<&OpticsSystemNode::on_destroy_Optics>(*this);
            //auto e = r.create();
            //r.emplace<Optics::Dirty>(e);
    });
}

void
OpticsSystemNode::updateOptics(VSGContext vsgcontext)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(target, void());

    auto update = [&](auto entity, auto& optics, auto& opticsDetail, auto& transformDetail)
    {
        opticsDetail.focalDistance = optics.focalDistance;
        opticsDetail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
        opticsDetail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;

        if (!optics.autoComputeFocalDistance)
            return;

        // Compose the optics' local pose onto the entity's world transform.
        // Assumes TransformDetail view 0 is representative for world-space pose.
        // TODO: MAKE ME MULTI-VIEW COMPATIBLE
        glm::dmat4 entityWorld = to_glm(transformDetail.views[0].model);
        glm::dmat4 opticsWorld = entityWorld * optics.pose;

        // Forward axis in world space. Convention here is camera/projector looks down -Z.
        glm::dvec3 forward = -glm::dvec3(opticsWorld[2]);
        double fwdLen = glm::length(forward);
        if (fwdLen <= 0.0)
            return;

        forward /= fwdLen;

        auto start = vsg::dvec3(opticsWorld[3][0], opticsWorld[3][1], opticsWorld[3][2]);
        auto end = start + to_vsg(forward * 1e8);

        vsg::LineSegmentIntersector lsi(start, end);
        target.ref_ptr()->accept(lsi);

        if (!lsi.intersections.empty())
        {
            auto closest = std::min_element(
                lsi.intersections.begin(), lsi.intersections.end(),
                [](const auto& lhs, const auto& rhs) { return lhs->ratio < rhs->ratio; });

            optics.focalPoint = to_glm(closest->get()->worldIntersection);
            optics.focalDistance = glm::length(optics.focalPoint - to_glm(start));

            opticsDetail.focalDistance = optics.focalDistance;

            // based on the angle of incidence and the intersection,
            // compute near and far clip distances for the optics frustum:
            if (optics.projection == Optics::Projection::Perspective)
            {
                opticsDetail.nearDistance = optics.focalDistance * optics.nearScale + optics.nearBias;
                opticsDetail.farDistance = optics.focalDistance * optics.farScale + optics.farBias;

                if (optics.autoComputeNearFar)
                {
                    // TODO: fix this hack and use a real up vector
                    // Terrain normal = local geocentric UP at the focal point.
                    glm::dvec3 terrainNormal = optics.focalPoint;
                    double nlen = glm::length(terrainNormal);
                    if (nlen > 0.0)
                        terrainNormal /= nlen;
                    else
                        terrainNormal = glm::dvec3(0.0, 0.0, 1.0);

                    auto clamp01 = [](double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); };

                    // alpha: center-ray angle from terrain normal (0=head-on, pi/2=grazing)
                    double cosAlpha = clamp01(glm::abs(glm::dot(forward, terrainNormal)));
                    cosAlpha = std::max(1e-6, cosAlpha);
                    double alpha = acos(cosAlpha);

                    // Frustum diagonal half-angle from center ray to a corner ray.
                    double tanHalfY = tan(glm::radians(optics.fovY * 0.5));
                    double tanHalfX = tanHalfY * optics.aspectRatio;
                    double coneHalfAngle = atan(std::sqrt(tanHalfX * tanHalfX + tanHalfY * tanHalfY));

                    // Eye-to-plane distance along terrain normal.
                    double h = optics.focalDistance * cosAlpha;

                    // Corner-ray incidence limits.
                    double nearAngle = alpha - coneHalfAngle;
                    double farAngle = alpha + coneHalfAngle;

                    // Keep far angle away from 90 deg singularity.
                    const double maxAngle = glm::radians(89.0);
                    if (farAngle > maxAngle) farAngle = maxAngle;

                    // Distance along ray to hit flat tangent plane.
                    // If nearAngle < 0, center ray is the closest valid hit.
                    double nearRaw =
                        (nearAngle <= 0.0) ? optics.focalDistance : (h / std::max(cos(nearAngle), 1e-6));
                    double farRaw = h / std::max(cos(farAngle), 1e-6);

                    // Padding and hard floors for stable clipping.
                    double nearPad = std::max(1.0, optics.focalDistance * 0.05);
                    double farPad = std::max(1.0, optics.focalDistance * 0.01);
                    double nearClip = std::max(1.0, nearRaw - nearPad);
                    double farClip = std::max(nearClip + 1.0, farRaw + farPad);

                    // Store ABSOLUTE distances (not offsets).
                    opticsDetail.nearDistance = nearClip;
                    opticsDetail.farDistance = farClip;
                }
            }
        }
    };

    _registry.write()->view<Optics, OpticsDetail, TransformDetail>().each(update);
}

void
OpticsSystemNode::initialize(VSGContext vsgcontext)
{
    //nop
}

void
OpticsSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    updateOptics(vsgcontext);

    Inherit::update(vsgcontext);
}
