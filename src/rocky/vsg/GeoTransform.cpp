/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"

using namespace ROCKY_NAMESPACE;

void
GeoTransform::dirty()
{
    for (auto& view : _transformDetail.views)
        view.revision = -1;
}

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    _transformDetail.update(record);

    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    if (_transformDetail.passingCull(rs))
    {
        _transformDetail.push(record);
        Inherit::traverse(record);
        _transformDetail.pop(record);
    }
}
