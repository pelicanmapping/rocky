#pragma once
#include <vsg/all.h>
#include <rocky/URI.h>
#include <rocky/vsg/RTT.h>
#include <rocky/vsg/ecs/MeshSystem.h>
#include "helpers.h"
#include <filesystem>

using namespace ROCKY_NAMESPACE;
namespace
{
    static VkImageUsageFlags computeUsageFlags(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        default:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    static vsg::ref_ptr<vsg::ImageView> createImageView(
        vsg::ref_ptr<vsg::Device> device,
        VkFormat format,
        const VkExtent2D& extent)
    {
        auto image = vsg::Image::create();
        image->format = format;
        image->extent = VkExtent3D{ extent.width, extent.height, 1 };
        image->mipLevels = 1;
        image->arrayLayers = 1;
        image->samples = VK_SAMPLE_COUNT_1_BIT;
        image->usage = computeUsageFlags(format) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        return vsg::createImageView(device, image, vsg::computeAspectFlagsForFormat(format));
    }

    static vsg::ref_ptr<vsg::RenderPass> createRenderPass(
        vsg::ref_ptr<vsg::Device> device,
        VkFormat imageFormat,
        VkFormat depthFormat)
    {
        auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        vsg::RenderPass::Attachments attachments{ colorAttachment, depthAttachment };

        vsg::AttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        vsg::AttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        vsg::SubpassDescription subpass = {};
        subpass.colorAttachments.emplace_back(colorAttachmentRef);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        vsg::RenderPass::Subpasses subpasses{ subpass };

        vsg::SubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        vsg::SubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        vsg::RenderPass::Dependencies dependencies{ colorDependency, depthDependency };
        return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
    }

    static vsg::ref_ptr<vsg::Framebuffer> createFramebuffer(
        vsg::ref_ptr<vsg::Device> device,
		const VkExtent2D& extent)
    {
        VkFormat imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        vsg::ImageViews imageViews;
        imageViews.emplace_back(createImageView(device, imageFormat, extent));
        imageViews.emplace_back(createImageView(device, depthFormat, extent));

        auto renderPass = createRenderPass(device, imageFormat, depthFormat);
        return vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);;
    }
}

static void screenshot(rocky::VSGContext ctx, vsg::ref_ptr<vsg::Image> sourceImage,vsg::Path&filename)
{
	auto width = sourceImage->extent.width;
	auto height = sourceImage->extent.height;

	auto device = ctx->device();
	auto physicalDevice = device->getPhysicalDevice();
	VkFormat sourceImageFormat = sourceImage->format;
	VkFormat targetImageFormat = sourceImageFormat;
	
	VkFormatProperties srcFormatProperties;
	vkGetPhysicalDeviceFormatProperties(*(physicalDevice), sourceImageFormat, &srcFormatProperties);

	VkFormatProperties destFormatProperties;
	vkGetPhysicalDeviceFormatProperties(*(physicalDevice), VK_FORMAT_R8G8B8A8_SRGB, &destFormatProperties);
	targetImageFormat = VK_FORMAT_R8G8B8A8_SRGB;

	auto destinationImage = vsg::Image::create();
	destinationImage->imageType = VK_IMAGE_TYPE_2D;
	destinationImage->format = targetImageFormat;
	destinationImage->extent.width = width;
	destinationImage->extent.height = height;
	destinationImage->extent.depth = 1;
	destinationImage->arrayLayers = 1;
	destinationImage->mipLevels = 1;
	destinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	destinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
	destinationImage->tiling = VK_IMAGE_TILING_LINEAR;
	destinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	destinationImage->compile(device);

	auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	destinationImage->bind(deviceMemory, 0);

	auto commands = vsg::Commands::create();
	auto transitionDestinationImageToDestinationLayoutBarrier = vsg::ImageMemoryBarrier::create(
		0,                                                             // srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,                                  // dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,                                     // oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // newLayout
		VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
		destinationImage,                                              // image
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
	);

	auto transitionSourceImageToTransferSourceLayoutBarrier = vsg::ImageMemoryBarrier::create(
		VK_ACCESS_MEMORY_READ_BIT,                                     // srcAccessMask
		VK_ACCESS_TRANSFER_READ_BIT,                                   // dstAccessMask
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // newLayout
		VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
		sourceImage,                                                   // image
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
	);

	auto cmd_transitionForTransferBarrier = vsg::PipelineBarrier::create(
		VK_PIPELINE_STAGE_TRANSFER_BIT,                       // srcStageMask
		VK_PIPELINE_STAGE_TRANSFER_BIT,                       // dstStageMask
		0,                                                    // dependencyFlags
		transitionDestinationImageToDestinationLayoutBarrier, // barrier
		transitionSourceImageToTransferSourceLayoutBarrier    // barrier
	);

	commands->addChild(cmd_transitionForTransferBarrier);

	VkImageBlit region{};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
	region.srcOffsets[1] = VkOffset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
	region.dstOffsets[1] = VkOffset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };

	auto blitImage = vsg::BlitImage::create();
	blitImage->srcImage = sourceImage;
	blitImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitImage->dstImage = destinationImage;
	blitImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitImage->regions.push_back(region);
	blitImage->filter = VK_FILTER_NEAREST;

	commands->addChild(blitImage);

	auto transitionDestinationImageToMemoryReadBarrier = vsg::ImageMemoryBarrier::create(
		VK_ACCESS_TRANSFER_WRITE_BIT,                                  // srcAccessMask
		VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                          // oldLayout
		VK_IMAGE_LAYOUT_GENERAL,                                       // newLayout
		VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
		destinationImage,                                              // image
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
	);

	auto transitionSourceImageBackToPresentBarrier = vsg::ImageMemoryBarrier::create(
		VK_ACCESS_TRANSFER_READ_BIT,                                   // srcAccessMask
		VK_ACCESS_MEMORY_READ_BIT,                                     // dstAccessMask
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,                          // oldLayout
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                               // newLayout
		VK_QUEUE_FAMILY_IGNORED,                                       // srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,                                       // dstQueueFamilyIndex
		sourceImage,                                                   // image
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourceRange
	);

	auto cmd_transitionFromTransferBarrier = vsg::PipelineBarrier::create(
		VK_PIPELINE_STAGE_TRANSFER_BIT,                // srcStageMask
		VK_PIPELINE_STAGE_TRANSFER_BIT,                // dstStageMask
		0,                                             // dependencyFlags
		transitionDestinationImageToMemoryReadBarrier, // barrier
		transitionSourceImageBackToPresentBarrier      // barrier
	);

	commands->addChild(cmd_transitionFromTransferBarrier);

	auto fence = vsg::Fence::create(device);
	auto queueFamilyIndex = physicalDevice->getQueueFamily(VK_QUEUE_GRAPHICS_BIT);
	auto commandPool = vsg::CommandPool::create(device, queueFamilyIndex);
	auto queue = device->getQueue(queueFamilyIndex);

	vsg::submitCommandsToQueue(commandPool, fence, 100000000000, queue, [&](vsg::CommandBuffer& commandBuffer) {
		commands->record(commandBuffer);
		});

	VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout subResourceLayout;
	vkGetImageSubresourceLayout(*device, destinationImage->vk(device->deviceID), &subResource, &subResourceLayout);

	size_t destRowWidth = width * sizeof(vsg::ubvec4);
	vsg::ref_ptr<vsg::Data> imageData;
	if (destRowWidth == subResourceLayout.rowPitch)
	{
		imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ targetImageFormat }, width, height); // deviceMemory, offset, flags and dimensions
	}
	else
	{
		auto mappedData = vsg::MappedData<vsg::ubyteArray>::create(deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{ targetImageFormat }, subResourceLayout.rowPitch * height);
		imageData = vsg::ubvec4Array2D::create(width, height, vsg::Data::Properties{ targetImageFormat });
		for (uint32_t row = 0; row < height; ++row)
		{
			std::memcpy(imageData->dataPointer(row * width), mappedData->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
		}
	}

	vsg::write(imageData, filename, ctx->readerWriterOptions);
}

auto Demo_Screenshot = [](Application& app)
{
    static bool isIniting = false;
    static vsg::ref_ptr<vsg::RenderGraph> offscreenRenderGraph;
    if (isIniting)
    {
        return;
    }

    if (!offscreenRenderGraph)
    {
        isIniting = true;
        auto main_window = app.display.mainWindow();
        auto main_view = app.display.views(main_window).front();
        auto vp = main_view->camera->getViewport();

        auto device = main_window->getDevice();
		VkExtent2D extent{ (std::uint32_t)vp.width, (std::uint32_t)vp.height };

        offscreenRenderGraph = vsg::RenderGraph::create();
        offscreenRenderGraph->framebuffer = createFramebuffer(device, extent);
        offscreenRenderGraph->renderArea.extent = offscreenRenderGraph->framebuffer->extent2D();
		offscreenRenderGraph->setClearValues(
			VkClearColorValue{ {0.0f, 0.0f, 0.0f, 1.0f} },
			VkClearDepthStencilValue{ 0.0f, 0 });

		auto viewRG = app.display.renderGraph(main_view);
		offscreenRenderGraph->children = viewRG->children;

        auto install = [&app, main_window](...)
            {
                auto commandGraph = app.display.commandGraph(main_window);
                if (commandGraph)
                {
                    commandGraph->children.insert(commandGraph->children.begin(), offscreenRenderGraph);
                    app.display.compileRenderGraph(offscreenRenderGraph, main_window);
                }
            };

        app.onNextUpdate(install);

        app.vsgcontext->requestFrame();
        isIniting = false;
        return;
    }

	static char filenameBuffer[512] = "screenshot.jpg";
	ImGui::TextUnformatted("Save Path:");
	ImGui::SameLine();
	ImGui::InputText("##Save Path", filenameBuffer, sizeof(filenameBuffer));

	if (ImGui::Button("Save"))
	{
		auto saveImage = [&app](...) {
			vsg::Path filename(filenameBuffer);
			auto imageViews = offscreenRenderGraph->framebuffer->getAttachments();
			screenshot(app.vsgcontext, imageViews[0]->image, filename);
			};

		app.onNextUpdate(saveImage);
		app.vsgcontext->requestFrame();
	}
};
