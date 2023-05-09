/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Icon.h"
#include "json.h"
#include <vsg/nodes/DepthSorted.h>

using namespace ROCKY_NAMESPACE;

Icon::Icon() :
    super()
{
    _bindStyle = BindIconStyle::create();
    _geometry = IconGeometry::create();

    this->horizonCulling = true;
    this->underGeoTransform = true;
}

void
Icon::setStyle(const IconStyle& value)
{
    _bindStyle->setStyle(value);
}

const IconStyle&
Icon::style() const
{
    return _bindStyle->style();
}

void
Icon::setImage(std::shared_ptr<Image> image)
{
    _bindStyle->setImage(image);
}

std::shared_ptr<Image>
Icon::image() const
{
    return _bindStyle->image();
}

void
Icon::createNode(Runtime& runtime)
{
    if (!node)
    {
        ROCKY_HARD_ASSERT(IconState::status.ok());

        auto stateGroup = vsg::StateGroup::create();
        stateGroup->stateCommands = IconState::pipelineStateCommands;
        stateGroup->addChild(_bindStyle);
        stateGroup->addChild(_geometry);
        auto sw = vsg::Switch::create();
        sw->addChild(true, stateGroup);
        node = sw;
    }
}

JSON
Icon::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}
