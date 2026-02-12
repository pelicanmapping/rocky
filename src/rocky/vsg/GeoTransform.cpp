/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"

using namespace ROCKY_NAMESPACE;

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    if (revision != _transformDetail.sync.revision)
    {
        _transformDetail.sync = *this;
    }

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
