/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Mesh.h"
#include "json.h"
#include "engine/Runtime.h"
#include <vsg/nodes/DepthSorted.h>

using namespace ROCKY_NAMESPACE;

Mesh::Mesh() :
    super()
{
    _geometry = MeshGeometry::create();
}

void
Mesh::dirty()
{
    if (_bindStyle)
    {
        // update the UBO with the new style data.
        if (style.has_value())
        {
            _bindStyle->updateStyle(style.value());
        }
    }
}

void
Mesh::createNode(Runtime& runtime)
{
    if (!node)
    {
        ROCKY_HARD_ASSERT(MeshState::status.ok());

        if (texture || style.has_value())
        {
            _bindStyle = BindMeshStyle::create();
            _bindStyle->_imageInfo = texture;
            dirty();
        }

        auto stateGroup = vsg::StateGroup::create();

        int features = 0;
        if (texture) features |= MeshState::TEXTURE;
        if (writeDepth) features |= MeshState::WRITE_DEPTH;
        if (_bindStyle) features |= MeshState::DYNAMIC_STYLE;

        auto& config = MeshState::get(features);
        stateGroup->stateCommands = config.pipelineStateCommands;

        if (_bindStyle)
        {
            _bindStyle->build(config.pipelineConfig->layout);
            stateGroup->addChild(_bindStyle);
        }

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
