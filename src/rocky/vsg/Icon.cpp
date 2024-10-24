/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Icon.h"
#include "json.h"
#include "engine/IconSystem.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

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
Icon::dirtyImage()
{
    node = nullptr;
}

int
Icon::featureMask() const
{
    return IconSystemNode::featureMask(*this);
}

JSON
Icon::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}
