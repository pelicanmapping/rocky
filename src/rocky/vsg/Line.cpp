/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Line.h"
#include "engine/LineSystem.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

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

int
Line::featureMask() const
{
    return LineSystemNode::featureMask(*this);
}

JSON
Line::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");

    auto j = json::parse(ECS::Component::to_json());
    // todo
    return j.dump();
}

