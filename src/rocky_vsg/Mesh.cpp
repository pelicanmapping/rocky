/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Mesh.h"
#include "json.h"
#include <vsg/nodes/DepthSorted.h>

using namespace ROCKY_NAMESPACE;

Mesh::Mesh() :
    super()
{
    _bindStyle = BindMeshStyle::create();
    _geometry = MeshGeometry::create();
}

void
Mesh::setStyle(const MeshStyle& value)
{
    _bindStyle->setStyle(value);
}

const MeshStyle&
Mesh::style() const
{
    return _bindStyle->style();
}

void
Mesh::createNode(Runtime& runtime)
{
    if (!node)
    {
        ROCKY_HARD_ASSERT(MeshState::status.ok());

        auto stateGroup = vsg::StateGroup::create();
        stateGroup->stateCommands = MeshState::pipelineStateCommands;
        stateGroup->addChild(_bindStyle);
        stateGroup->addChild(_geometry);
        auto sw = vsg::Switch::create();
        sw->addChild(true, stateGroup);
        node = sw;
    }
}

JSON
Mesh::to_json() const
{
    ROCKY_SOFT_ASSERT(false, "Not yet implemented");
    auto j = json::object();
    set(j, "name", name);
    return j.dump();
}
