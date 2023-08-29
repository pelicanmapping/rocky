/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Line.h"
#include "engine/LineSystem.h"
#include "json.h"
#include <vsg/nodes/CullNode.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/StateGroup.h>

using namespace ROCKY_NAMESPACE;

Line::Line()
{
    bindCommand = BindLineDescriptors::create();
}

void
Line::dirty()
{
    if (bindCommand)
    {
        // update the UBO with the new style data.
        if (style.has_value())
        {
            bindCommand->updateStyle(style.value());
        }
    }
}

void
Line::initializeNode(const ECS::NodeComponent::Params& params)
{
    auto cull = vsg::CullNode::create();

    if (style.has_value())
    {
        bindCommand = BindLineDescriptors::create();
        dirty();
        bindCommand->init(params.layout);

        auto sg = vsg::StateGroup::create();
        sg->stateCommands.push_back(bindCommand);
        for (auto& g : geometries)
            sg->addChild(g);
        cull->child = sg;
    }
    else if (geometries.size() == 1)
    {
        cull->child = geometries[0];
    }
    else
    {
        auto group = vsg::Group::create();
        for (auto& g : geometries)
            group->addChild(g);
        cull->child = group;
    }

    vsg::ComputeBounds cb;
    cull->child->accept(cb);
    cull->bound.set((cb.bounds.min + cb.bounds.max) * 0.5, vsg::length(cb.bounds.min - cb.bounds.max) * 0.5);

    node = cull;
}

int
Line::featureMask() const
{
    return LineSystem::featureMask(*this);
}

JSON
Line::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");

    auto j = json::parse(ECS::Component::to_json());
    // todo
    return j.dump();
}

