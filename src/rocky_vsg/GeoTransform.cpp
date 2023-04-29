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

    auto state = rv.getState();

    // pass the LTP matrix down to children in case they need it
    state->setValue("local_to_world", view.matrix);

    // replicates RecordTraversal::accept(MatrixTransform&):

    state->modelviewMatrixStack.push(state->modelviewMatrixStack.top() * view.matrix);
    state->dirty = true;

    state->pushFrustum();
    vsg::CullGroup::accept(rv);
    state->popFrustum();

    state->modelviewMatrixStack.pop();
    state->dirty = true;
}



void
HorizonCullGroup::accept(vsg::RecordTraversal& rv) const
{
    shared_ptr<Horizon> horizon;
    if (rv.getState()->getValue("horizon", horizon))
    {
        vsg::dmat4 m;
        if (rv.getState()->getValue("local_to_world", m))
        {
            if (!horizon->isVisible(m[3][0], m[3][1], m[3][2]))
                return;
        }
    }
    vsg::Group::accept(rv);
}
