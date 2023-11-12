/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/state/ImageInfo.h>
#include <vsg/vk/Context.h>

namespace ROCKY_VSG_NAMESPACE
{
    /**
    * Utilities for render-to-texture.
    */
    class ROCKY_EXPORT RTT
    {
    public:
        //! Create a rendergraph that renders to a texture. Insert this render graph
        //!   under a CommandGraph to activate it.
        //! @param context VSG context to use
        //! @param extent Extents of the render surface
        //! @param colorImageInfo If not null, RTT will generate a color texture in this object
        //! @param depthImageInfo If not null, RTT will generate a depth texture in this object.
        //!   Note, you still need a depth texture even if you only want color but still need
        //!   the render to use depth testing!
        static vsg::ref_ptr<vsg::RenderGraph> createOffScreenRenderGraph(
            vsg::Context& context,
            const VkExtent2D& extent,
            vsg::ref_ptr<vsg::ImageInfo> colorImageInfo,
            vsg::ref_ptr<vsg::ImageInfo> depthImageInfo);
    };
}
