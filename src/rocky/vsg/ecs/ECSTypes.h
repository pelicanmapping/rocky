/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/Common.h>
#include <rocky/ecs/Component.h>

namespace ROCKY_NAMESPACE
{
    /** ECS component that holds a VSG node */
    struct NodeGraph : public ComponentBase
    {
        vsg::ref_ptr<vsg::Node> node;
    };


    /** ECS Component that holds a VSG sampler/texture */
    struct MeshTexture : public ComponentBase2<MeshTexture>
    {
        vsg::ref_ptr<vsg::ImageInfo> imageInfo;
    };
}
