/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Icon.h"
#include "json.h"
#include "engine/IconSystem.h"

using namespace ROCKY_NAMESPACE;

Icon::Icon()
{
    geometry = IconGeometry::create();
}

void
Icon::dirty()
{
    if (bindCommand)
    {
        // update the UBO with the new style data.
        bindCommand->updateStyle(style);
    }
}

void
Icon::initializeNode(const ECS::NodeComponent::Params& params)
{
    bindCommand = BindIconStyle::create();
    bindCommand->_image = image;
    dirty();
    bindCommand->init(params.layout);

    auto stateGroup = vsg::StateGroup::create();
    stateGroup->stateCommands.push_back(bindCommand);
    stateGroup->addChild(geometry);
    node = stateGroup;
}

int
Icon::featureMask() const
{
    return IconSystem::featureMask(*this);
}

JSON
Icon::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}
