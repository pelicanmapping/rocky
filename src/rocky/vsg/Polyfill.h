/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include <rocky/vsg/Common.h>

namespace ROCKY_NAMESPACE
{    class SetPolygonMode : public vsg::Inherit<vsg::StateCommand, SetPolygonMode>
    {
    public:
        SetPolygonMode(vsg::Device* device, VkPolygonMode pm) : _polygonMode(pm) {
            device->getProcAddr<PFN_vkCmdSetPolygonModeEXT>(_vkCmdSetPolygonMode, "vkCmdSetPolygonModeEXT");
        }
        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_vkCmdSetPolygonMode)
                _vkCmdSetPolygonMode(commandBuffer, _polygonMode);
        }
    private:
        PFN_vkCmdSetPolygonModeEXT _vkCmdSetPolygonMode = nullptr;
        VkPolygonMode _polygonMode;
    };


    class SetDepthWriteEnable : public vsg::Inherit<vsg::StateCommand, SetDepthWriteEnable>
    {
    public:
        SetDepthWriteEnable(vsg::Device* device, VkBool32 value) : _enable(value) {
            device->getProcAddr<PFN_vkCmdSetDepthWriteEnableEXT>(_vkCmdSetDepthWriteEnable, "vkCmdSetDepthWriteEnableEXT");
        }
        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_vkCmdSetDepthWriteEnable)
                _vkCmdSetDepthWriteEnable(commandBuffer, _enable);
        }
    private:
        VkBool32 _enable;
        PFN_vkCmdSetDepthWriteEnableEXT _vkCmdSetDepthWriteEnable = nullptr;
    };


    class SetCullMode : public vsg::Inherit<vsg::StateCommand, SetCullMode>
    {
    public:
        SetCullMode(vsg::Device* device, VkCullModeFlags value) : _cullMode(value) {
            device->getProcAddr<PFN_vkCmdSetCullModeEXT>(_vkCmdSetCullMode, "vkCmdSetCullModeEXT");
        }
        void record(vsg::CommandBuffer& commandBuffer) const override {
            if (_vkCmdSetCullMode)
                _vkCmdSetCullMode(commandBuffer, _cullMode);
        }
    private:
        VkCullModeFlags _cullMode;
        PFN_vkCmdSetCullModeEXT _vkCmdSetCullMode = nullptr;
    };
}
