/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"
#include "MapNode.h"
#include "Utils.h"

using namespace ROCKY_NAMESPACE;

GeoTransform::GeoTransform()
{
   //nop
}

const GeoPoint&
GeoTransform::position() const
{
    return _position;
}

void
GeoTransform::setPosition(const GeoPoint& position)
{
    if (position != _position)
    {
        _position = position;

        // do we need to mutex this?
        for (auto& view : _viewlocal)
            view.dirty = true;
    }
}

void
GeoTransform::accept(vsg::RecordTraversal& rv) const
{
    // get the view-local data:
    auto& view = _viewlocal[rv.getState()->_commandBuffer->viewID];

    if (view.dirty)
    {
        SRS worldSRS;
        if (rv.getValue("worldsrs", worldSRS))
        {
            if (_position.transform(worldSRS, view.worldPos))
            {
                view.matrix = to_vsg(worldSRS.localToWorldMatrix(view.worldPos.to_dvec3()));
            }
        }

        view.dirty = false;
    }
    
    // replicates RecordTraversal::accept(MatrixTransform&):

    auto state = rv.getState();

    state->modelviewMatrixStack.push(state->modelviewMatrixStack.top() * view.matrix);
    state->dirty = true;

    state->pushFrustum();
    vsg::Group::accept(rv);
    state->popFrustum();

    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
