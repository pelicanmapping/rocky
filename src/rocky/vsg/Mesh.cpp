/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Mesh.h"
#include "json.h"
#include "engine/MeshSystem.h"
#include "engine/Runtime.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

Mesh::Mesh()
{
    geometry = MeshGeometry::create();
}

void
Mesh::dirty()
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
Mesh::featureMask() const
{
    return MeshSystemNode::featureMask(*this);
}

JSON
Mesh::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");

    auto j = json::parse(ECS::Component::to_json());
    // todo
    return j.dump();
}
