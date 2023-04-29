/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Mesh.h"
#include "json.h"

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
        node = stateGroup;
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
