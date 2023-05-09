/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "MapObject.h"
#include "engine/Runtime.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    // generates a unique ID for each map object
    std::atomic_uint32_t uid_generator(0U);
}

void
Attachment::setVisible(bool value)
{
    if (node && !node->children.empty())
    {
        node->children.front().mask = value ? vsg::MASK_ALL : vsg::MASK_OFF;
    }
}

bool
Attachment::visible() const
{
    return
        node &&
        !node->children.empty() &&
        node->children.front().mask != vsg::MASK_OFF;
}

MapObject::MapObject() :
    super(),
    uid(uid_generator++)
{
    root = vsg::Group::create();

    xform = GeoTransform::create();
    root->addChild(xform);
}

MapObject::MapObject(shared_ptr<Attachment> value) :
    MapObject()
{
    attachments = { value };
}

MapObject::MapObject(Attachments value) :
    MapObject()
{
    attachments = value;
}
