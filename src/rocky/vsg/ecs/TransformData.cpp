/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TransformData.h"
#include "../Utils.h"

using namespace ROCKY_NAMESPACE;

void
TransformViewData::update(vsg::RecordTraversal& record)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(transform, void());

    if (!transform->position.valid())
        return;

    auto* state = record.getState();

    // only if something has changed since last time:
    if (revision != transform->revision)
    {
        revision = transform->revision;

        // first time through, cache information about the world SRS and ellipsoid for this view.
        if (!pos_to_world.valid())
        {
            if (record.getValue("rocky.worldsrs", world_srs))
            {
                pos_to_world = transform->position.srs.to(world_srs);
                world_ellipsoid = &world_srs.ellipsoid(); // for speed :)
            }
        }

        if (pos_to_world.valid())
        {
            glm::dvec3 worldpos;
            if (pos_to_world(transform->position, worldpos))
            {
                if (transform->topocentric && world_srs.isGeocentric())
                {
                    model = to_vsg(world_ellipsoid->topocentricToGeocentricMatrix(worldpos));
                }
                else
                {
                    model = vsg::translate(worldpos.x, worldpos.y, worldpos.z);
                }

                if (ROCKY_MAT4_IS_NOT_IDENTITY(transform->localMatrix))
                {
                    glm::dmat4 temp;
                    ROCKY_FAST_MAT4_MULT(temp, model, transform->localMatrix);
                    model = to_vsg(temp);
                }
            }
        }

        if (!horizon)
        {
            // cache this view's horizon pointer in the local view data
            // so we don't have to look it up every frame
            record.getValue("rocky.horizon", horizon);
        }

        //local = to_vsg(transform->localMatrix);
    }

    proj = state->projectionMatrixStack.top();
    auto& mvm = state->modelviewMatrixStack.top();
    ROCKY_FAST_MAT4_MULT(modelview, mvm, model);
    ROCKY_FAST_MAT4_MULT(mvp, proj, modelview);

    viewport = (*state->_commandBuffer->viewDependentState->viewportData)[0];
    //culled = false;
}

bool
TransformViewData::passesCull() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(transform, true);

    // Frustum cull (by center point)
    if (transform->frustumCulled)
    {
        auto clip = mvp[3] / mvp[3][3];
        const double t = 1.0;
        if (clip.x < -t || clip.x > t || clip.y < -t || clip.y > t || clip.z < -t || clip.z > t)
        {
            //view.culled = true;
            return false;
        }
    }

    // horizon cull, if active (geocentric only)
    if (transform->horizonCulled && horizon && world_srs.isGeocentric())
    {
        if (!horizon->isVisible(model[3][0], model[3][1], model[3][2], transform->radius))
        {
            //view.culled = true;
            return false;
        }
    }

    return true;
}

void
TransformViewData::push(vsg::RecordTraversal& record) const
{
    auto* state = record.getState();

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(modelview);
    state->dirty = true;
    state->pushFrustum();
}

void
TransformViewData::pop(vsg::RecordTraversal& record) const
{
    auto state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
