/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"
#include "Utils.h"
#include <rocky/Horizon.h>
#include <vsg/state/ViewDependentState.h>

using namespace ROCKY_NAMESPACE;


GeoTransform::GeoTransform()
{
    //nop
}

void
GeoTransform::setPosition(const GeoPoint& position_)
{
    position = position_;
    dirty();
}

void
GeoTransform::dirty()
{
    for (auto& view : viewLocal)
        view.dirty = true;
}

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    // traverse the transform
    if (push(record, true))
    {
        Inherit::traverse(record);
        pop(record);
    }
}

bool
GeoTransform::push(vsg::RecordTraversal& record, bool cull, const std::optional<vsg::dmat4>& localMatrix) const
{
    if (!position.valid())
    {
        return false;
    }

    auto* state = record.getState();

    // fetch the view-local data:
    auto& view = viewLocal[state->_commandBuffer->viewID];

    bool update_required =
        view.dirty ||
        (localMatrix.has_value() && !ROCKY_MAT4_EQUAL(localMatrix.value(), view.local));

    // only if something has changed since last time:
    if (update_required)
    {
        // first time through, cache information about the world SRS and ellipsoid for this view.
        if (!view.pos_to_world.valid())
        {
            if (record.getValue("rocky.worldsrs", view.world_srs))
            {
                view.pos_to_world = position.srs.to(view.world_srs);
                view.world_ellipsoid = &view.world_srs.ellipsoid(); // for speed :)
            }
        }

        if (view.pos_to_world.valid())
        {
            glm::dvec3 worldpos;
            if (view.pos_to_world(glm::dvec3(position.x, position.y, position.z), worldpos))
            {
                if (localTangentPlane && view.world_srs.isGeocentric())
                    view.model = to_vsg(view.world_ellipsoid->topocentricToGeocentricMatrix(worldpos));
                else
                    view.model = vsg::translate(worldpos.x, worldpos.y, worldpos.z);

                if (localMatrix.has_value())
                {
                    glm::dmat4 temp;
                    ROCKY_FAST_MAT4_MULT(temp, view.model, localMatrix.value());
                    view.model = to_vsg(temp);
                }
            }
        }

        if (localMatrix.has_value())
        {
            view.local = localMatrix.value();
        }

        view.dirty = false;
    }

    view.proj = state->projectionMatrixStack.top();
    auto& modelview = state->modelviewMatrixStack.top();
    ROCKY_FAST_MAT4_MULT(view.modelview, modelview, view.model);

    view.viewport = (*state->_commandBuffer->viewDependentState->viewportData)[0];
    view.culled = false;

    if (!cull)
    {
        return false;
    }

    // Frustum cull (by center point)
    if (frustumCulling)
    {
        vsg::mat4 mvp;
        ROCKY_FAST_MAT4_MULT(mvp, view.proj, view.modelview);

        vsg::vec4 clip = mvp[3] / mvp[3][3];
        const double t = 1.0;
        if (clip.x < -t || clip.x > t || clip.y < -t || clip.y > t || clip.z < -t || clip.z > t)
        {
            view.culled = true;
            return false;
        }
    }

    // horizon cull, if active (geocentric only)
    if (horizonCulling && view.world_srs.isGeocentric())
    {
        if (!view.horizon)
        {
            // cache this view's horizon pointer in the local view data
            // so we don't have to look it up every frame
            record.getValue("rocky.horizon", view.horizon);
        }

        if (view.horizon)
        {
            if (!view.horizon->isVisible(view.model[3][0], view.model[3][1], view.model[3][2], bound.radius))
            {
                view.culled = true;
                return false;
            }
        }
    }

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(view.modelview);
    state->dirty = true;
    state->pushFrustum();

    return true;
}

void
GeoTransform::pop(vsg::RecordTraversal& record) const
{
    auto state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
