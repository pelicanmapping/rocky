#include "RTT.h"

using namespace ROCKY_VSG_NAMESPACE;


// adapted from vsgExamples/vsgrendertotexture.cpp

vsg::ref_ptr<vsg::RenderGraph> RTT::createOffScreenRenderGraph(
    vsg::Context& context,
    const VkExtent2D& extent,
    vsg::ImageInfo& colorImageInfo,
    vsg::ImageInfo& depthImageInfo)
{
    auto device = context.device;

    VkExtent3D attachmentExtent{ extent.width, extent.height, 1 };

    // Attachments
    // create image for color attachment
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = VK_FORMAT_R8G8B8A8_UNORM;
    colorImage->extent = attachmentExtent;
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->flags = 0;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto colorImageView = createImageView(context, colorImage, VK_IMAGE_ASPECT_COLOR_BIT);

    // Sampler for accessing attachment as a texture
    auto colorSampler = vsg::Sampler::create();
    colorSampler->flags = 0;
    colorSampler->magFilter = VK_FILTER_LINEAR;
    colorSampler->minFilter = VK_FILTER_LINEAR;
    colorSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    colorSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    colorSampler->addressModeV = colorSampler->addressModeU;
    colorSampler->addressModeW = colorSampler->addressModeU;
    colorSampler->mipLodBias = 0.0f;
    colorSampler->maxAnisotropy = 1.0f;
    colorSampler->minLod = 0.0f;
    colorSampler->maxLod = 1.0f;
    colorSampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    colorImageInfo.imageView = colorImageView;
    colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorImageInfo.sampler = colorSampler;

    // create depth buffer
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = attachmentExtent;
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->flags = 0;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // XXX Does layout matter?
    depthImageInfo.sampler = nullptr;
    depthImageInfo.imageView = vsg::createImageView(context, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // attachment descriptions
    vsg::RenderPass::Attachments attachments(2);
    // Color attachment
    attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    vsg::AttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    vsg::RenderPass::Subpasses subpassDescription(1);
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].colorAttachments.emplace_back(colorReference);
    subpassDescription[0].depthStencilAttachments.emplace_back(depthReference);

    vsg::RenderPass::Dependencies dependencies(2);

    // XXX This dependency is copied from the offscreenrender.cpp
    // example. I don't completely understand it, but I think it's
    // purpose is to create a barrier if some earlier render pass was
    // using this framebuffer's attachment as a texture.
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This is the heart of what makes Vulkan offscreen rendering
    // work: render passes that follow are blocked from using this
    // passes' color attachment in their fragment shaders until all
    // this pass' color writes are finished.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    auto renderPass = vsg::RenderPass::create(device, attachments, subpassDescription, dependencies);

    // Framebuffer
    auto fbuf = vsg::Framebuffer::create(renderPass, vsg::ImageViews{ colorImageInfo.imageView, depthImageInfo.imageView }, extent.width, extent.height, 1);

    auto rendergraph = vsg::RenderGraph::create();
    rendergraph->renderArea.offset = VkOffset2D{ 0, 0 };
    rendergraph->renderArea.extent = extent;
    rendergraph->framebuffer = fbuf;

    rendergraph->clearValues.resize(2);
    rendergraph->clearValues[0].color = { {1.0f, 0.3f, 0.4f, 1.0f} };
    rendergraph->clearValues[1].depthStencil = VkClearDepthStencilValue{ 0.0f, 0 };

    return rendergraph;
}
