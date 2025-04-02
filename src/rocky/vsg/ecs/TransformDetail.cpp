/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TransformDetail.h"
#include "../Utils.h"

using namespace ROCKY_NAMESPACE;

bool
TransformDetail::update(vsg::RecordTraversal& record)
{
    auto& view = views[record.getState()->_commandBuffer->viewID];

    if (!sync.position.valid())
        return false;

    auto* state = record.getState();

    // only if something has changed since last time:
    bool transform_changed = (view.revision != sync.revision);

    if (transform_changed)
    {
        view.revision = sync.revision; // transform->revision;

        // first time through, cache information about the world SRS and ellipsoid for this view.
        if (!pos_to_world.valid())
        {
            if (record.getValue("rocky.worldsrs", world_srs))
            {
                pos_to_world = sync.position.srs.to(world_srs);
                world_ellipsoid = &world_srs.ellipsoid(); // for speed :)
            }
        }

        if (pos_to_world.valid())
        {
            glm::dvec3 worldpos;
            if (pos_to_world(sync.position, worldpos))
            {
                if (sync.topocentric && world_srs.isGeocentric())
                {
                    view.model = to_vsg(world_ellipsoid->topocentricToGeocentricMatrix(worldpos));
                }
                else
                {
                    view.model = vsg::translate(worldpos.x, worldpos.y, worldpos.z);
                }

                if (ROCKY_MAT4_IS_NOT_IDENTITY(sync.localMatrix))
                {
                    glm::dmat4 temp;
                    ROCKY_FAST_MAT4_MULT(temp, view.model, sync.localMatrix);
                    view.model = to_vsg(temp);
                }
            }
        }

        if (!horizon)
        {
            // cache this view's horizon pointer in the local view data
            // so we don't have to look it up every frame
            record.getValue("rocky.horizon", horizon);
        }
    }

    view.proj = state->projectionMatrixStack.top();
    auto& mvm = state->modelviewMatrixStack.top();
    ROCKY_FAST_MAT4_MULT(view.modelview, mvm, view.model);
    ROCKY_FAST_MAT4_MULT(view.mvp, view.proj, view.modelview);

    view.viewport = (*state->_commandBuffer->viewDependentState->viewportData)[0];

    return transform_changed;
}

bool
TransformDetail::passesCull(vsg::RecordTraversal& record) const
{
    return passesCull(record.getState()->_commandBuffer->viewID);
}

bool
TransformDetail::passesCull(std::uint32_t viewID) const
{
    //ROCKY_SOFT_ASSERT_AND_RETURN(transform, true);

    auto& view = views[viewID];

    // Frustum cull (by center point)
    if (sync.frustumCulled)
    {
        auto clip = view.mvp[3] / view.mvp[3][3];
        const double t = 1.0;
        if (clip.x < -t || clip.x > t || clip.y < -t || clip.y > t || clip.z < -t || clip.z > t)
        {
            return false;
        }
    }

    // horizon cull, if active (geocentric only)
    if (sync.horizonCulled && horizon && world_srs.isGeocentric())
    {
        if (!horizon->isVisible(view.model[3][0], view.model[3][1], view.model[3][2], sync.radius))
        {
            return false;
        }
    }

    return true;
}

void
TransformDetail::push(vsg::RecordTraversal& record) const
{
    auto& view = views[record.getState()->_commandBuffer->viewID];

    auto* state = record.getState();

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(view.modelview);
    state->dirty = true;
    state->pushFrustum();
}

void
TransformDetail::pop(vsg::RecordTraversal& record) const
{
    auto state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
