/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"
#include "Utils.h"
#include "ecs/TransformSystem.h"
#include <rocky/Horizon.h>
#include <vsg/state/ViewDependentState.h>

using namespace ROCKY_NAMESPACE;


GeoTransform::GeoTransform()
{
    for(auto& view : transformData)
        view.transform = this;
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
    for (auto& view : transformData)
        view.revision = -1;
}

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    auto& view = transformData[record.getState()->_commandBuffer->viewID];

    view.update(record);

    if (view.passesCull())
    {
        view.push(record);
        Inherit::traverse(record);
        view.pop(record);
    }
}
