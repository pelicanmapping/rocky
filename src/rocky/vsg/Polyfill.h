/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <vsg/vk/DeviceExtensions.h>

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


    //! Records dynamic depth testing directly in a style pass, without a VSG state-stack slot.
    class SetDepthTestEnable : public vsg::Inherit<vsg::Command, SetDepthTestEnable>
    {
    public:
        //! Captures the value; the recording device must support dynamic depth-test enable.
        explicit SetDepthTestEnable(VkBool32 value) : _enable(value) {}

        //! Records on the current device; the bound pipeline must declare depth-test enable dynamic.
        void record(vsg::CommandBuffer& commandBuffer) const override {
            commandBuffer.getDevice()->getExtensions()->vkCmdSetDepthTestEnable(commandBuffer, _enable);
        }
    private:
        VkBool32 _enable;
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

EVSG_type_name(rocky::VulkanExtensions)
EVSG_type_name(rocky::SetPolygonMode)
EVSG_type_name(rocky::SetDepthTestEnable)
EVSG_type_name(rocky::SetDepthWriteEnable)
EVSG_type_name(rocky::SetCullMode)
EVSG_type_name(rocky::SetColorWriteMask)
