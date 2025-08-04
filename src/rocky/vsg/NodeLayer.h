/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/VisibleLayer.h>
#include <rocky/vsg/VSGContext.h>

namespace ROCKY_NAMESPACE
{
    class ROCKY_EXPORT NodeLayer : public Inherit<rocky::VisibleLayer, NodeLayer>
    {
    public:
        //! Default construct
        NodeLayer() = default;

        //! Construct with a node
        NodeLayer(vsg::ref_ptr<vsg::Node> in_node) :
            super(), node(in_node) {
        }

        vsg::ref_ptr<vsg::Node> node;
    };
}
