/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "TransformDetail.h"
#include "../VSGUtils.h"

using namespace ROCKY_NAMESPACE;

bool
TransformDetail::update(vsg::RecordTraversal& record)
{
    auto viewID = record.getCommandBuffer()->viewID;
    auto& view = views[viewID];

    if (!sync.position.valid())
        return false;

    auto* state = record.getState();

    // only if something has changed since last time:
    bool transform_changed = (view.revision != sync.revision);

    if (transform_changed)
    {
        view.revision = sync.revision;

        // first time through, cache information about the world SRS and ellipsoid for this view.
        if (!cached.pos_to_world)
        {
            if (record.getValue("rocky.worldsrs", cached.world_srs))
            {
                cached.pos_to_world = sync.position.srs.to(cached.world_srs);
                cached.world_ellipsoid = &cached.world_srs.ellipsoid(); // for speed :)
            }
        }

        if (cached.pos_to_world)
        {
            glm::dvec3 worldpos;
            if (cached.pos_to_world(sync.position, worldpos))
            {
                if (sync.topocentric && cached.world_srs.isGeocentric())
                {
                    view.model = to_vsg(cached.world_ellipsoid->topocentricToGeocentricMatrix(worldpos));
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

        if (!cached.horizon)
        {
            // cache this view's horizon pointer in the local view data
            // so we don't have to look it up every frame
            record.getValue("rocky.horizon", cached.horizon);
        }
    }

    view.proj = state->projectionMatrixStack.top();
    auto& mvm = state->modelviewMatrixStack.top();
    ROCKY_FAST_MAT4_MULT(view.modelview, mvm, view.model);
    ROCKY_FAST_MAT4_MULT(view.mvp, view.proj, view.modelview);

    view.viewport = (*state->_commandBuffer->viewDependentState->viewportData)[0];


    view.passingCull = true;

    // Frustum cull (by center point)
    if (sync.frustumCulled)
    {
        auto clip = view.mvp[3] / view.mvp[3][3];

        double tx = 1.0, ty = 1.0, tz = 1.0;
        if (sync.radius > 0.0)
        {
            auto rv = view.modelview[3] + vsg::dvec4(sync.radius, sync.radius, 0, 0);
            auto rc = (view.proj * rv);

            tx += std::abs((rc.x / rc.w) - clip.x);
            ty += std::abs((rc.y / rc.w) - clip.y);
        }

        if (clip.x < -tx || clip.x > tx || clip.y < -ty || clip.y > ty || clip.z < -tz || clip.z > tz)
        {
            view.passingCull = false;
        }
    }

    // horizon cull, if active (geocentric only)
    if (view.passingCull && sync.horizonCulled && cached.horizon && cached.world_srs.isGeocentric())
    {
        auto& horizon = *cached.horizon;
        if (!horizon[viewID].isVisible(view.model[3][0], view.model[3][1], view.model[3][2], sync.radius))
        {
            view.passingCull = false;
        }
    }

    return transform_changed;
}

void
TransformDetail::push(vsg::RecordTraversal& record) const
{
    auto& view = views[record.getCommandBuffer()->viewID];

    auto* state = record.getState();

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(view.modelview);
    state->dirty = true;
    state->pushFrustum();
}

void
TransformDetail::pop(vsg::RecordTraversal& record) const
{
    auto* state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
