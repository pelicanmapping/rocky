/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "GeoTransform.h"

using namespace ROCKY_NAMESPACE;

void
GeoTransform::setPosition(const GeoPoint& position_)
{
    position = position_;
    dirty();
}

void
GeoTransform::dirty()
{
    for (auto& view : transform_detail.views)
        view.revision = -1;
}

void
GeoTransform::traverse(vsg::RecordTraversal& record) const
{
    transform_detail.update(record);

    ViewRecordingState vrs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount
    };

    if (transform_detail.visible(vrs))
    {
        transform_detail.push(record);
        Inherit::traverse(record);
        transform_detail.pop(record);
    }
}
