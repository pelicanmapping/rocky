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
    if (position != position_)
    {
        position = position_;
        dirty();
    }
}

void
GeoTransform::dirty()
{
    for (auto& view : _viewlocal)
        view.dirty = true;
}

void
GeoTransform::accept(vsg::RecordTraversal& record) const
{
    // traverse the transform
    if (push(record, vsg::dmat4(1.0)))
    {
        vsg::Group::accept(record);
        pop(record);
    }
}

bool
GeoTransform::push(vsg::RecordTraversal& record, const vsg::dmat4& local_matrix) const
{
    auto state = record.getState();

    // update the view-local data if necessary:
    auto& view = _viewlocal[record.getState()->_commandBuffer->viewID];
    if (view.dirty || local_matrix != view.local_matrix)
    {
        if (!view.pos_to_world.valid())
        {
            if (record.getValue("worldsrs", view.world_srs))
            {
                view.pos_to_world = position.srs.to(view.world_srs);
            }
        }

        if (view.pos_to_world.valid())
        {
            glm::dvec3 worldpos;
            if (view.pos_to_world(glm::dvec3(position.x, position.y, position.z), worldpos))
            {              
                view.matrix = to_vsg(view.world_srs.ellipsoid().geocentricToLocalToWorld(worldpos)) * local_matrix;
            }
        }

        view.local_matrix = local_matrix;
        view.dirty = false;
    }

    // horizon cull, if active:
    if (horizonCulling)
    {
        std::shared_ptr<Horizon> horizon;
        if (state->getValue("horizon", horizon))
        {
            if (!horizon->isVisible(view.matrix[3][0], view.matrix[3][1], view.matrix[3][2], bound.radius))
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
