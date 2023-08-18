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
    auto sw = node ? node->cast<vsg::Switch>() : nullptr;
    if (sw && !sw->children.empty())
    {
        sw->children.front().mask = value ? vsg::MASK_ALL : vsg::MASK_OFF;
    }
}

bool
Attachment::visible() const
{
    auto sw = node ? node->cast<vsg::Switch>() : nullptr;
    return
        sw &&
        !sw->children.empty() &&
        sw->children.front().mask != vsg::MASK_OFF;
}

void
AttachmentGroup::createNode(Runtime& runtime)
{
    if (!node)
    {
        auto group = vsg::Group::create();
        
        for (auto& attachment : attachments)
        {
            attachment->createNode(runtime);
            if (attachment->node)
            {
                group->addChild(attachment->node);
            }
        }

        auto sw = vsg::Switch::create();
        sw->addChild(true, group);
        node = sw;
    }
}

JSON
AttachmentGroup::to_json() const
{
    json j = json::object();
    set(j, "name", name);

    json children = json::array();
    for (auto& att : attachments)
    {
        json c = json::parse(att->to_json());
        if (!c.empty())
            children.push_back(c);
    }
    if (!children.empty())
        set(j, "attachments", children);

    return j.dump();
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

const GeoPoint&
MapObject::position() const
{
    static GeoPoint empty;
    return xform ? xform->position() : empty;
}