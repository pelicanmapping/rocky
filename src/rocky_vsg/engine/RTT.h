/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Common.h>
#include <vsg/app/RenderGraph.h>
#include <vsg/state/ImageInfo.h>
#include <vsg/vk/Context.h>

namespace ROCKY_VSG_NAMESPACE
{
    class ROCKY_VSG_EXPORT RTT
    {
    public:
        static vsg::ref_ptr<vsg::RenderGraph> createOffScreenRenderGraph(
            vsg::Context& context,
            const VkExtent2D& extent,
            vsg::ImageInfo& colorImageInfo,
            vsg::ImageInfo& depthImageInfo);
    };
}
