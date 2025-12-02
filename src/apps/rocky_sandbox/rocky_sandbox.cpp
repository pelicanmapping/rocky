/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <rocky/rocky.h>

#include <unordered_map>

// Basic design for a deferred rendering pipeline in VSG and Rocky.
//
// Objects:
//   Window
//     CommandGraph
//        Channel
//        Channel (inset)
//        ...
//   Window
//      CommandGraph
//        Channel
//        ...

using namespace ROCKY_NAMESPACE;


#if 1
// A single attachment -- a raster source/target in the workflow.
class Attachment
{
public:
    std::string name;
    vsg::ref_ptr<vsg::ImageView> imageView;
    vsg::AttachmentDescription description;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    //std::uint32_t index = 0;
};

using AttachmentVector = std::vector<Attachment>;

using AttachmentTable = std::unordered_map<std::string, Attachment>;


// A "channel" represents a workflow applied to a viewport within a window.
class Channel : public vsg::Inherit<vsg::Group, Channel>
{
public:
    Channel(vsg::Window* in_window, vsg::View* in_view) :
        Inherit(), window(in_window), view(in_view) {
    }

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::View> view;
};


// One step in a render workflow.
// This will usually be a rendergraph for drawing or compute, or a barrier.
class Stage : public vsg::Inherit<vsg::Object, Stage>
{
public:
    std::string name;

    // output attachments created by this stage
    AttachmentVector outputAttachments;

    virtual AttachmentVector createAttachments(Channel* channel) {
        return {};
    }

    virtual vsg::ref_ptr<vsg::Node> createNode(
        Channel* channel, const AttachmentTable& attachments) = 0;
};


// A "chain" of work stages that assemble a renderable frame.
// Each stage can render, compute, or set a barrier.
class ViewWorkflow : public vsg::Inherit<vsg::Group, ViewWorkflow>
{
public:
    ViewWorkflow(vsg::Window* in_window, vsg::View* in_view) :
        window(in_window), view(in_view) {}
    
    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::View> view;
    std::vector<vsg::ref_ptr<Stage>> stages;
    
    AttachmentTable attachments; // all attachments used in this workflow, keyed by name


    //! Generates the graph to render this view to the swapchain.
    void build()
    {
        vsg::Context cx(window->getOrCreateDevice());

        auto channel = Channel::create(window, view);

        // collect the attachments from each stage
        attachments.clear();
        for (auto& stage : stages)
        {
            auto stageAttachments = stage->createAttachments(channel);

            for (auto& a : stageAttachments)
            {
                attachments[a.name] = a;
            }
        }

        // build the actual node graph for each stage and add it
        for (auto& stage : stages)
        {
            auto node = stage->createNode(channel, attachments);
            if (node)
            {
                this->addChild(node);
            }
        }
    }
};


// then we can design various stages.

class RenderToGBuffer : public vsg::Inherit<Stage, RenderToGBuffer>
{
    auto createAttachments(Channel* channel) -> AttachmentVector override
    {
        AttachmentVector output;

        auto extent = channel->view->camera->getRenderArea().extent;

        vsg::Context cx(channel->window->getOrCreateDevice());

        // Albedo attachment:
        {
            auto image = vsg::Image::create();
            image->imageType = VK_IMAGE_TYPE_2D;
            image->format = VK_FORMAT_R8G8B8A8_UNORM;
            image->extent = VkExtent3D{ extent.width, extent.height, 1U };
            image->mipLevels = 1;
            image->arrayLayers = 1;
            image->samples = VK_SAMPLE_COUNT_1_BIT;
            image->tiling = VK_IMAGE_TILING_OPTIMAL;
            image->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image->flags = 0;
            image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            auto imageView = vsg::createImageView(cx, image, VK_IMAGE_ASPECT_COLOR_BIT);

            vsg::AttachmentDescription description;
            description.format = VK_FORMAT_R8G8B8A8_UNORM;
            description.samples = VK_SAMPLE_COUNT_1_BIT;
            description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            output.emplace_back(Attachment{
                "albedo", 
                imageView,
                std::move(description),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        }

        // Depth attachment:
        {
            auto image = vsg::Image::create();
            image->imageType = VK_IMAGE_TYPE_2D;
            image->format = VK_FORMAT_D32_SFLOAT;
            image->extent = VkExtent3D{ extent.width, extent.height, 1U };
            image->mipLevels = 1;
            image->arrayLayers = 1;
            image->samples = VK_SAMPLE_COUNT_1_BIT;
            image->tiling = VK_IMAGE_TILING_OPTIMAL;
            image->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image->flags = 0;
            image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            auto imageView = vsg::createImageView(cx, image, VK_IMAGE_ASPECT_DEPTH_BIT);

            vsg::AttachmentDescription description;
            description.format = VK_FORMAT_D32_SFLOAT;
            description.samples = VK_SAMPLE_COUNT_1_BIT;
            description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            output.emplace_back(Attachment{
                "depth",
                imageView,
                std::move(description),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
        }

        return output;
    }

    auto createNode(Channel* channel, const AttachmentTable& attachments)
        -> vsg::ref_ptr<vsg::Node> override
    { 
        // build RenderPass from attachments
        vsg::RenderPass::Attachments renderPassAttachments;
        vsg::RenderPass::Subpasses subpassDescriptions(1);
        vsg::RenderPass::Dependencies dependencies;
        vsg::ImageViews imageViews;

        vsg::Context cx(channel->window->getOrCreateDevice());

        // color attachment
        std::uint32_t index = 0;
        {
            const auto& albedoAttachment = attachments.at("albedo");
            renderPassAttachments.push_back(albedoAttachment.description);
            vsg::AttachmentReference colorReference{ 
                index++, //albedoAttachment.index,
                albedoAttachment.layout };
            subpassDescriptions[0].colorAttachments.push_back(colorReference);
            imageViews.emplace_back(albedoAttachment.imageView);
        }

        // depth attachment
        {
            const auto& depthAttachment = attachments.at("depth");
            renderPassAttachments.push_back(depthAttachment.description);
            vsg::AttachmentReference depthReference{
                index++, // depthAttachment.index,
                depthAttachment.layout };
            subpassDescriptions[0].depthStencilAttachments.push_back(depthReference);
            imageViews.emplace_back(depthAttachment.imageView);
        }

        auto renderPass = vsg::RenderPass::create(cx.device, renderPassAttachments, subpassDescriptions, dependencies);

        // Framebuffer
        auto extent = channel->view->camera->getRenderArea().extent;
        auto framebuffer = vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);

        // Build the actual RenderGraph to render the scene to our framebuffer
        auto renderGraph = vsg::RenderGraph::create();
        renderGraph->framebuffer = framebuffer;
        renderGraph->clearValues.resize(2);
        renderGraph->clearValues[0].color = vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{ 0.0f, 0 };
        renderGraph->addChild(channel->view);
        return renderGraph;
    }
};

class BlitGBufferToSwapChain : public vsg::Inherit<Stage, BlitGBufferToSwapChain>
{
public:
    auto createNode(Channel* channel, const AttachmentTable& attachments)
        -> vsg::ref_ptr<vsg::Node> override
    {
        auto renderGraph = vsg::RenderGraph::create(channel->window);
        renderGraph->addChild(channel->view);
        // create a FSQ for the final blit, and 
        return renderGraph;
    }
};
#endif



void install_debug_layer(vsg::ref_ptr<vsg::Window>);
bool debugLayer = false;

int main(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    if (args.read("--debug"))
        debugLayer = true;

    auto windowTraits = vsg::WindowTraits::create(1920, 1080, argv[0]);

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);

    auto scene = vsg::read_cast<vsg::Node>("H:/devel/vsg/vsgexamples/repo/data/models/teapot.vsgt");
    vsg::ComputeBounds computeBounds;
    scene->accept(computeBounds);

    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    // set up the camera
    double nearFarRatio = 0.0001;
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 10.5);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // the view that uses this camera
    auto view = vsg::View::create(camera);

    // build the workflow graph for our view:
    auto workflow = ViewWorkflow::create(window, view);
    workflow->stages.emplace_back(RenderToGBuffer::create());
    workflow->stages.emplace_back(BlitGBufferToSwapChain::create());
    workflow->build();

    // install the view workflow on the command graph:
    auto cg = vsg::CommandGraph::create(window);
    cg->children.emplace_back(workflow);

    //auto cg = vsg::createCommandGraphForView(window, camera, scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({ cg });
    viewer->compile({});

    // add close handler to respond to the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}




// https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/debug_utils
VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        rocky::Log()->warn(std::string_view(callback_data->pMessage));
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        rocky::Log()->error(std::string_view(callback_data->pMessage));
    }
    return VK_FALSE;
}


void install_debug_layer(vsg::ref_ptr<vsg::Window> window)
{
    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

    static VkDebugUtilsMessengerEXT debug_utils_messenger;

    auto vki = window->getOrCreateDevice()->getInstance();

    using PFN_vkCreateDebugUtilsMessengerEXT = VkResult(VKAPI_PTR*)(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*, const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    if (vki->getProcAddr(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessenger", "vkCreateDebugUtilsMessengerEXT"))
    {
        vkCreateDebugUtilsMessengerEXT(vki->vk(), &debug_utils_create_info, nullptr, &debug_utils_messenger);
    }
}