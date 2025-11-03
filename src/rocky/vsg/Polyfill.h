/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>

namespace ROCKY_NAMESPACE
{  
    class ROCKY_EXPORT VulkanExtensions : public vsg::Inherit<vsg::Object, VulkanExtensions>
    {
    public:
        VulkanExtensions(vsg::Device*);

        PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonMode = nullptr;
        PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnable = nullptr;
        PFN_vkCmdSetCullModeEXT vkCmdSetCullMode = nullptr;
        PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMask = nullptr;
    };


    class SetPolygonMode : public vsg::Inherit<vsg::StateCommand, SetPolygonMode>
    {
    public:
        SetPolygonMode(VulkanExtensions* ext, VkPolygonMode pm) : _ext(ext), _polygonMode(pm) { }

        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_ext->vkCmdSetPolygonMode)
                _ext->vkCmdSetPolygonMode(commandBuffer, _polygonMode);
        }
    private:
        vsg::ref_ptr<VulkanExtensions> _ext;
        VkPolygonMode _polygonMode;
    };


    class SetDepthWriteEnable : public vsg::Inherit<vsg::StateCommand, SetDepthWriteEnable>
    {
    public:
        SetDepthWriteEnable(VulkanExtensions* ext, VkBool32 value) : _ext(ext), _enable(value) {}

        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_ext->vkCmdSetDepthWriteEnable)
                _ext->vkCmdSetDepthWriteEnable(commandBuffer, _enable);
        }
    private:
        vsg::ref_ptr<VulkanExtensions> _ext;
        VkBool32 _enable;
    };


    class SetCullMode : public vsg::Inherit<vsg::StateCommand, SetCullMode>
    {
    public:
        SetCullMode(VulkanExtensions* ext, VkCullModeFlags value) : _ext(ext), _cullMode(value) {}

        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_ext->vkCmdSetCullMode)
                _ext->vkCmdSetCullMode(commandBuffer, _cullMode);
        }
    private:
        vsg::ref_ptr<VulkanExtensions> _ext;
        VkCullModeFlags _cullMode;
    };


    class SetColorWriteMask : public vsg::Inherit<vsg::StateCommand, SetColorWriteMask>
    {
    public:
        SetColorWriteMask(VulkanExtensions* ext, VkColorComponentFlags value) : _ext(ext), _colorWriteMask(value) {}

        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_ext->vkCmdSetColorWriteMask)
                _ext->vkCmdSetColorWriteMask(commandBuffer, 0, 1, &_colorWriteMask);
        }
    private:
        vsg::ref_ptr<VulkanExtensions> _ext;
        VkColorComponentFlags _colorWriteMask;
    };
}
