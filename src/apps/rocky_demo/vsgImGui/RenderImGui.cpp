/* <editor-fold desc="MIT License">

Copyright(c) 2021 Don Burns, Roland Hill and Robert Osfield.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "RenderImGui.h"

#include <imgui_impl_vulkan.h>

#include <vsg/io/Logger.h>
#include <vsg/vk/State.h>
#include <vsg/vk/SubmitCommands.h>

using namespace vsgImGui;

namespace vsgImGui
{
    void check_vk_result(VkResult err)
    {
        if (err == 0) return;

        vsg::error("[vulkan] Error: VkResult = ", err);
    }

    class ImGuiNode : public vsg::Inherit<vsg::Node, ImGuiNode>
    {
    public:
        ImGuiNode(RenderImGui::LegacyFunction in_func) :
            func(in_func) {}

        RenderImGui::LegacyFunction func;

        void accept(vsg::RecordTraversal&) const override
        {
            func();
        }
    };

} // namespace vsgImGui

RenderImGui::RenderImGui(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments)
{
    _init(window, useClearAttachments);
    _uploadFonts();
}

RenderImGui::RenderImGui(vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
                         vsg::ref_ptr<vsg::RenderPass> renderPass,
                         uint32_t minImageCount, uint32_t imageCount,
                         VkExtent2D imageSize, bool useClearAttachments)
{
    _init(device, queueFamily, renderPass, minImageCount, imageCount, imageSize, useClearAttachments);
    _uploadFonts();
}

RenderImGui::~RenderImGui()
{
    ImGui_ImplVulkan_Shutdown();
    //ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void RenderImGui::add(const LegacyFunction& legacyFunc)
{
    addChild(ImGuiNode::create(legacyFunc));
}

void RenderImGui::_init(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments)
{
    auto device = window->getOrCreateDevice();
    auto physicalDevice = device->getPhysicalDevice();

    uint32_t queueFamily = 0;
    std::tie(queueFamily, std::ignore) = physicalDevice->getQueueFamily(window->traits()->queueFlags, window->getSurface());
    auto queue = device->getQueue(queueFamily);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physicalDevice,
                                              *(window->getSurface()),
                                              &capabilities);
    uint32_t imageCount = 3;
    imageCount =
        std::max(imageCount,
                 capabilities.minImageCount); // Vulkan spec requires
                                              // minImageCount to be 1 or greater
    if (capabilities.maxImageCount > 0)
        imageCount = std::min(
            imageCount,
            capabilities.maxImageCount); // Vulkan spec specifies 0 as being
                                         // unlimited number of images

    _init(device, queueFamily, window->getOrCreateRenderPass(), capabilities.minImageCount, imageCount, window->extent2D(), useClearAttachments);
}

void RenderImGui::_init(
    vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
    vsg::ref_ptr<vsg::RenderPass> renderPass,
    uint32_t minImageCount, uint32_t imageCount,
    VkExtent2D imageSize, bool useClearAttachments)
{
    IMGUI_CHECKVERSION();
    if (!ImGui::GetCurrentContext()) ImGui::CreateContext();
    //if (!ImPlot::GetCurrentContext()) ImPlot::CreateContext();

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    for (auto& attachment : renderPass->attachments)
    {
        if (attachment.samples > samples) samples = attachment.samples;
    }

    // ImGui may change this later, but ensure the display
    // size is set to something, to prevent assertions
    // in ImGui::newFrame.
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = static_cast<float>(imageSize.width);
    io.DisplaySize.y = static_cast<float>(imageSize.height);

    _device = device;
    _queueFamily = queueFamily;
    _queue = _device->getQueue(_queueFamily);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *(_device->getInstance());
    init_info.PhysicalDevice = *(_device->getPhysicalDevice());
    init_info.Device = *(_device);
    init_info.QueueFamily = _queueFamily;
    init_info.Queue = *(_queue); // ImGui doesn't use the queue so we shouldn't need to assign it, but it has an IM_ASSERT requiring it during debug build.
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.MSAASamples = samples;

    // Create Descriptor Pool
    vsg::DescriptorPoolSizes pool_sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    uint32_t maxSets = static_cast<uint32_t>(1000 * pool_sizes.size());
    _descriptorPool = vsg::DescriptorPool::create(_device, maxSets, pool_sizes);

    init_info.DescriptorPool = *_descriptorPool;
    init_info.RenderPass = *renderPass;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = std::max(minImageCount, 2u); // ImGui's Vulkan backend has an assert that requires MinImageCount to be 2 or more.
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = check_vk_result;

    ImGui_ImplVulkan_Init(&init_info);

    if (useClearAttachments)
    {
        // clear the depth buffer before view2 gets rendered
        VkClearValue clearValue{};
        clearValue.depthStencil = {1.0f, 0};
        VkClearAttachment attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearValue};
        VkClearRect rect{VkRect2D{VkOffset2D{0, 0}, VkExtent2D{imageSize.width, imageSize.height}}, 0, 1};
        _clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect});
    }
}

void RenderImGui::_uploadFonts()
{
    ImGui_ImplVulkan_CreateFontsTexture();
}

void RenderImGui::accept(vsg::RecordTraversal& rt) const
{
    auto& commandBuffer = *(rt.getState()->_commandBuffer);
    if (_device.get() != commandBuffer.getDevice()) return;

    // record all the ImGui commands to ImDrawData container
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    // traverse children
    traverse(rt);

    //ImGui::EndFrame(); // called automatically by Render() below
    ImGui::Render();

    // if ImDrawData has been recorded then we need to clear the frame buffer and do the final record to Vulkan command buffer.
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0)
    {
        if (_clearAttachments) _clearAttachments->record(commandBuffer);

        if (draw_data)
            ImGui_ImplVulkan_RenderDrawData(draw_data, &(*commandBuffer));
    }
}

void RenderImGui::frame(std::function<void()> renderFunction)
{
    if (renderFunction)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
        renderFunction();
        ImGui::Render();
    }
}
