/**
 * rocky c++
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <rocky/rocky.h>
#include <rocky/vsg/PipelineState.h>
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
    vsg::ref_ptr<vsg::ImageInfo> imageInfo;
    vsg::AttachmentDescription description;
    VkImageLayout layout;

    vsg::ref_ptr<vsg::Descriptor> createDescriptor(std::uint32_t binding, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) const
    {
        return vsg::DescriptorImage::create(imageInfo, binding, 0, type);
    }
};

using AttachmentVector = std::vector<Attachment>;

using AttachmentTable = std::unordered_map<std::string, Attachment>;


// A "channel" represents a workflow applied to a viewport within a window.
class Channel : public vsg::Inherit<vsg::Group, Channel>
{
public:
    Channel(VSGContext in_vsgcontext, vsg::Window* in_window, vsg::View* in_view) :
        Inherit(), vsgcontext(in_vsgcontext), window(in_window), view(in_view) {
    }

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::View> view;
    VSGContext vsgcontext;
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
    ViewWorkflow(VSGContext in_vsgcontext, vsg::Window* in_window, vsg::View* in_view) :
        vsgcontext(in_vsgcontext), window(in_window), view(in_view) {}
    
    VSGContext vsgcontext;
    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::View> view;
    std::vector<vsg::ref_ptr<Stage>> stages;
    
    AttachmentTable attachments; // all attachments used in this workflow, keyed by name


    //! Generates the graph to render this view to the swapchain.
    void build()
    {
        vsg::Context cx(window->getOrCreateDevice());

        auto channel = Channel::create(vsgcontext, window, view);

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


vsg::ref_ptr<vsg::ShaderSet> createGBufferShaderSet(VSGContext& vsg)
{
    // load shaders
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT, "main",
        vsg::findFile("shaders/rocky.dr.standard.vert", vsg->searchPaths),
        vsg->readerWriterOptions);

    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main",
        vsg::findFile("shaders/rocky.dr.standard.frag", vsg->searchPaths),
        vsg->readerWriterOptions);

    if (!vertexShader || !fragmentShader)
        return { };

    auto shaderSet = vsg::ShaderSet::create(
        vsg::ShaderStages{ vertexShader, fragmentShader });

    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", 2, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", 3, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", 4, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", 5, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_Color", "", 6, VK_FORMAT_R32G32B32A32_SFLOAT, {});

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    // We need VSG's view-dependent data for lighting support
    PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_FRAGMENT_BIT);

    return shaderSet;
}

vsg::ref_ptr<vsg::Node> createGBufferPipeline(vsg::ShaderSet* shaderSet, VSGContext vsg)
{
    auto gc = vsg::GraphicsPipelineConfigurator::create(vsg::ref_ptr<vsg::ShaderSet>(shaderSet));

    gc->shaderHints = vsg->shaderCompileSettings ?
        vsg::ShaderCompileSettings::create(*vsg->shaderCompileSettings) :
        vsg::ShaderCompileSettings::create();

    gc->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gc->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gc->enableArray("vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, 8);

    PipelineUtils::enableViewDependentData(gc);

    struct SetPipelineStates : public vsg::Visitor
    {
        void apply(vsg::Object& object) override {
            object.traverse(*this);
        }
        void apply(vsg::RasterizationState& state) override {
            state.cullMode = VK_CULL_MODE_BACK_BIT;
        }
        void apply(vsg::DepthStencilState& state) override {
        }
        void apply(vsg::ColorBlendState& state) override {
            state.attachments = vsg::ColorBlendState::ColorBlendAttachments{
                { true,
                  VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                  VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
                  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
            };
        }
    };

    SetPipelineStates visitor;
    gc->accept(visitor);
    gc->init();

    auto commands = vsg::Commands::create();
    commands->addChild(gc->bindGraphicsPipeline);

    commands->addChild(vsg::BindViewDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        gc->layout,
        VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));

    return commands;
}



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

            auto sampler = vsg::Sampler::create();
            sampler->flags = 0;
            sampler->magFilter = VK_FILTER_LINEAR;
            sampler->minFilter = VK_FILTER_LINEAR;
            sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeV = sampler->addressModeU;
            sampler->addressModeW = sampler->addressModeU;
            sampler->mipLodBias = 0.0f;
            sampler->maxAnisotropy = 1.0f;
            sampler->minLod = 0.0f;
            sampler->maxLod = 1.0f;
            sampler->borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

            auto imageInfo = vsg::ImageInfo::create();
            imageInfo->imageView = imageView;
            imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo->sampler = sampler;

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
                imageInfo,
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

            auto sampler = vsg::Sampler::create();
            sampler->flags = 0;
            sampler->magFilter = VK_FILTER_LINEAR;
            sampler->minFilter = VK_FILTER_LINEAR;
            sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler->addressModeV = sampler->addressModeU;
            sampler->addressModeW = sampler->addressModeU;
            sampler->mipLodBias = 0.0f;
            sampler->maxAnisotropy = 1.0f;
            sampler->minLod = 0.0f;
            sampler->maxLod = 1.0f;

            auto imageInfo = vsg::ImageInfo::create();
            imageInfo->imageView = imageView;
            imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo->sampler = sampler;

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
                imageInfo,
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
            vsg::AttachmentReference attref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
            subpassDescriptions[0].colorAttachments.push_back(attref);
            imageViews.emplace_back(albedoAttachment.imageInfo->imageView);
        }

        // depth attachment
        {
            const auto& depthAttachment = attachments.at("depth");
            renderPassAttachments.push_back(depthAttachment.description);
            vsg::AttachmentReference attref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
            subpassDescriptions[0].depthStencilAttachments.push_back(attref);
            imageViews.emplace_back(depthAttachment.imageInfo->imageView);
        }

        auto renderPass = vsg::RenderPass::create(cx.device, renderPassAttachments, subpassDescriptions, dependencies);

        // Framebuffer
        auto extent = channel->view->camera->getRenderArea().extent;
        auto framebuffer = vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);

        // Build the actual RenderGraph to render the scene to our framebuffer
        auto renderGraph = vsg::RenderGraph::create();
        renderGraph->framebuffer = framebuffer;
        renderGraph->renderArea.offset = VkOffset2D{ 0, 0 };
        renderGraph->renderArea.extent = extent;

        renderGraph->clearValues.resize(2);
        renderGraph->clearValues[0].color = vsg::vec4(1.0f, 0.3f, 0.4f, 1.0f); // pink
        renderGraph->clearValues[1].depthStencil = VkClearDepthStencilValue{ 0.0f, 0 };

        renderGraph->addChild(channel->view);
        return renderGraph;
        return renderGraph;
    }
};



class RenderToFullScreenQuad : public vsg::Inherit<Stage, RenderToFullScreenQuad>
{
public:
    auto createNode(Channel* channel, const AttachmentTable& attachments)
        -> vsg::ref_ptr<vsg::Node> override
    {
        auto vsg = channel->vsgcontext;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT, "main",
            vsg::findFile("shaders/rocky.dr.fsq.vert", vsg->searchPaths),
            vsg->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT, "main",
            vsg::findFile("shaders/rocky.dr.fsq.frag", vsg->searchPaths),
            vsg->readerWriterOptions);

        if (!vertexShader || !fragmentShader)
            return { };

        // We need VSG's view-dependent data for lighting support
        //PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_FRAGMENT_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        //shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        auto vertexInputState = vsg::VertexInputState::create();

        auto inputAssemblyState = vsg::InputAssemblyState::create();
        inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        auto rasterizationState = vsg::RasterizationState::create();
        rasterizationState->cullMode = VK_CULL_MODE_NONE;

        auto depthStencilState = vsg::DepthStencilState::create();
        depthStencilState->depthTestEnable = VK_FALSE;

        auto viewportState = vsg::ViewportState::create(channel->window->extent2D());

        auto multisampleState = vsg::MultisampleState::create();

        auto colorBlendState = vsg::ColorBlendState::create();
        colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
            { false,
              VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
              VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
              VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
        };

        auto pipelineLayout = vsg::PipelineLayout::create();

        vsg::DescriptorSetLayoutBindings bindings{
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // albedo
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // depth
            { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}  // normal
        };
        auto dsetLayout = vsg::DescriptorSetLayout::create(bindings);

        pipelineLayout->setLayouts.emplace_back(dsetLayout);

        auto pipeline = vsg::GraphicsPipeline::create(
            pipelineLayout,
            vsg::ShaderStages{ vertexShader, fragmentShader },
            vsg::GraphicsPipelineStates{
                vertexInputState,
                inputAssemblyState,
                rasterizationState,
                multisampleState,
                colorBlendState,
                viewportState });

        auto bindPipeline = vsg::BindGraphicsPipeline::create(pipeline);

        auto commands = vsg::Group::create();
        commands->addChild(bindPipeline);


        // add the descriptors.
        auto albedoDS = attachments.at("albedo").createDescriptor(0);
        auto depthDS = attachments.at("depth").createDescriptor(1);

        auto dset = vsg::DescriptorSet::create(dsetLayout, vsg::Descriptors{ albedoDS, depthDS });
        commands->addChild(vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, dset));

        // draw the fsq.
        commands->addChild(vsg::Draw::create(4, 1, 0, 0)); // full-screen quad tristrip
        commands->addChild(vsg::createHeadlight()); // won't render without one

        auto view = vsg::View::create(vsg::Camera::create(), commands);
        auto renderGraph = vsg::RenderGraph::create(channel->window, view);

        return renderGraph;
    }
};
#endif


int fail(std::string_view msg) {
    rocky::Log()->critical(msg);
    exit(-1);
    return -1;
}

void install_debug_layer(vsg::ref_ptr<vsg::Window>);
bool debugLayer = false;



#if 0
int simple(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(1920, 1080, argv[0]);
    auto window = vsg::Window::create(windowTraits);

    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    VSGContext vsg = VSGContextFactory::create(viewer, argc, argv);

    // Use the existing pipeline creation function
    auto commands = createRenderToFSQPipeline(window, vsg);
    if (!commands || commands->children.empty())
        return -1;

    commands->addChild(vsg::Draw::create(4, 1, 0, 0)); // full-screen quad tristrip
    commands->addChild(vsg::createHeadlight()); // won't render without one

    auto view = vsg::View::create(vsg::Camera::create(), commands);
    auto rg = vsg::RenderGraph::create(window, view);
    auto cg = vsg::CommandGraph::create(window, rg);

    viewer->assignRecordAndSubmitTaskAndPresentation({ cg });
    viewer->compile({});

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}
#endif



vsg::ref_ptr<vsg::Node> loadScene(VSGContext vsg)
{
    vsg::Builder builder;
    builder.shaderSet = createGBufferShaderSet(vsg);
    if (!builder.shaderSet)
        fail("createGBufferShaderSet returned null shader set");

    vsg::GeometryInfo gi(vsg::box(vsg::vec3(-1, -1, -1), vsg::vec3(1, 1, 1)));
    gi.color = to_vsg(Color::Lime);
    vsg::StateInfo si;
    si.lighting = false, si.wireframe = true;
    auto scene = builder.createBox(gi, si);

    return scene;
}

vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* node, vsg::Window* window)
{
    vsg::ComputeBounds computeBounds;
    node->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    // set up the camera
    double nearFarRatio = 0.0001;
    double nearPlane = nearFarRatio * radius;
    double farPlane = radius * 10.0;
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearPlane, farPlane);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));
    return camera;
}



int main(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    auto windowTraits = vsg::WindowTraits::create(1920, 1080, argv[0]);
    if (args.read("--api"))
        windowTraits->apiDumpLayer = true;

    auto window = vsg::Window::create(windowTraits);

    if (args.read("--debug"))
        install_debug_layer(window);

    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    auto vsgcontext = VSGContextFactory::create(viewer, argc, argv);

    auto scene = loadScene(vsgcontext);
    auto camera = createCameraForScene(scene, window);

    // the view that uses this camera
    auto view = vsg::View::create(camera);
    view->addChild(scene);
    view->addChild(vsg::createHeadlight()); // won't work with it :/



    // build the workflow graph for our view:
    auto workflow = ViewWorkflow::create(vsgcontext, window, view);

    auto renderToGBuffer = RenderToGBuffer::create();
    if (!renderToGBuffer)
        return fail("RenderToGBuffer::create() returned null");
    workflow->stages.emplace_back(renderToGBuffer);

    auto renderToFSQ = RenderToFullScreenQuad::create();
    if (!renderToFSQ)
        return fail("RenderToFullScreenQuad::create() returned null");
    workflow->stages.emplace_back(renderToFSQ);

    // builds the scene graph for each render stage
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