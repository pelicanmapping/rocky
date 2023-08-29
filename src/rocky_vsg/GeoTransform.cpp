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
   //nop
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
    if (push(record))
    {
        vsg::Group::accept(record);
        pop(record);
    }
}

bool
GeoTransform::push(vsg::RecordTraversal& record) const
{
    auto state = record.getState();

    // update the view-local data if necessary:
    auto& view = _viewlocal[record.getState()->_commandBuffer->viewID];
    if (view.dirty)
    {
        SRS worldSRS;
        if (record.getValue("worldsrs", worldSRS))
        {
            if (position.transform(worldSRS, view.worldPos))
            {
                view.matrix = to_vsg(worldSRS.localToWorldMatrix(glm::dvec3(
                    view.worldPos.x, view.worldPos.y, view.worldPos.z)));
            }
        }

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
