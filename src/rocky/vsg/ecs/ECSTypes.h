/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/ecs/Component.h>
#include <rocky/vsg/ecs/Texture.h>

namespace ROCKY_NAMESPACE
{
    /** ECS component that holds a VSG node */
    struct NodeGraph : public Component<NodeGraph>
    {
        vsg::ref_ptr<vsg::Node> node;
    };
}
