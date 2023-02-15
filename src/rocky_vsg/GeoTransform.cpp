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
        for (auto& vdd : _vdd)
            vdd.dirty = true;
    }
}

void
GeoTransform::accept(vsg::RecordTraversal& rv) const
{
    // grow the VDD if necessary.
    auto viewID = rv.getState()->_commandBuffer->viewID;
    if (viewID >= _vdd.size())
    {
        std::scoped_lock lock(_mutex);
        if (viewID >= _vdd.size())
        {
            _vdd.resize(viewID + 1);
        }
    }

    auto& vdd = _vdd[viewID];

    // TODO: we will have to do this per-camera.
    if (vdd.dirty)
    {
        SRS worldSRS;
        if (rv.getValue("worldsrs", worldSRS))
        {
            if (_position.transform(worldSRS, vdd.worldPos))
            {
                vdd.matrix = to_vsg(worldSRS.localToWorldMatrix(vdd.worldPos.to_dvec3()));
            }
        }

        vdd.dirty = false;
    }

    auto state = rv.getState();

    state->modelviewMatrixStack.push(state->modelviewMatrixStack.top() * vdd.matrix);
    state->dirty = true;

    state->pushFrustum();
    vsg::Group::accept(rv);
    state->popFrustum();

    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
