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

OpticsSystemNode::OpticsSystemNode(Registry& registry) :
    Inherit(registry)
{
    //nop
}

void
OpticsSystemNode::updateOptics(VSGContext vsgcontext)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(target, void());

    auto update = [&](auto entity, auto& optics, auto& transformDetail)
    {
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

            //optics.focalDistance = vsg::length(closest->get()->worldIntersection - start);
            optics.focalPoint = to_glm(closest->get()->worldIntersection);
            optics.focalDistance = glm::length(optics.focalPoint - to_glm(start));
        }
    };

    _registry.write()->view<Optics, TransformDetail>().each(update);
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
