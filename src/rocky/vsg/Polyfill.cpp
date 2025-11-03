/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include "Polyfill.h"

using namespace ROCKY_NAMESPACE;

VulkanExtensions::VulkanExtensions(vsg::Device* device)
{
    ROCKY_HARD_ASSERT(device);

    device->getProcAddr<PFN_vkCmdSetPolygonModeEXT>(vkCmdSetPolygonMode, "vkCmdSetPolygonModeEXT");
    device->getProcAddr<PFN_vkCmdSetDepthWriteEnableEXT>(vkCmdSetDepthWriteEnable, "vkCmdSetDepthWriteEnableEXT");
    device->getProcAddr<PFN_vkCmdSetCullModeEXT>(vkCmdSetCullMode, "vkCmdSetCullModeEXT");
    device->getProcAddr<PFN_vkCmdSetColorWriteMaskEXT>(vkCmdSetColorWriteMask, "vkCmdSetColorWriteMaskEXT");
}
