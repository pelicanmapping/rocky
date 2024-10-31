/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"
#include "engine/Utils.h"
#include <rocky/Horizon.h>

using namespace ROCKY_NAMESPACE;

GeoTransform::GeoTransform()
{
    // force creation of the first viewlocal data structure
    _viewlocal[0].dirty = true;
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
    for (auto& view : _viewlocal)
        view.dirty = true;
}

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    // traverse the transform
    if (push(record, vsg::dmat4(1.0)))
    {
        Inherit::traverse(record);
        pop(record);
    }
}

bool
GeoTransform::push(vsg::RecordTraversal& record, const vsg::dmat4& local_matrix) const
{
    auto state = record.getState();

    // fetch the view-local data:
    auto& view = _viewlocal[record.getState()->_commandBuffer->viewID];

    // only if something has changed since last time:
    if (view.dirty || local_matrix != view.local_matrix)
    {
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
                    view.matrix = to_vsg(view.world_ellipsoid->geocentricToLocalToWorld(worldpos)) * local_matrix; 
                else
                    view.matrix = vsg::translate(worldpos.x, worldpos.y, worldpos.z) * local_matrix;                
            }
        }

        view.local_matrix = local_matrix;
        view.dirty = false;
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
            if (!view.horizon->isVisible(view.matrix[3][0], view.matrix[3][1], view.matrix[3][2], bound.radius))
                return false;
        }
    }

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(state->modelviewMatrixStack.top() * view.matrix);
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
