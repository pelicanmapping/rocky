/**
 * Copyright 2025 Pelican Mapping
 * MIT License
 */

#include <vsg/all.h>
#include <unordered_map>
#include <iostream>

// Deferred rendering sandbox for VSG.
//
// A deferred rendering pipeline consists of multiple Stages organized into
// a workflow. The first stage typically writes to a g-buffer, which is just
// a set of render target attachments; one for color, one for normals, one for
// depth.. etc. Subsequent stages can then read from these channels to perform
// post-processing before outputing the final result to the swapchain.
//
// ViewWorkflow is the top-level scene graph object, which will live under a 
// CommandGraph. As the name implies you will need one workflow for each unique
// vsg::View in the application.
//
// How to do it:
//  - create a ViewWorkflow object
//  - create various Stage objects and add each one to the ViewWorkflow
//  - call ViewWorkflow::build to assemble the scene graph
//  - add your ViewWorkflow to a command graph.
//
// Creating a Stage:
//  - the Stage subclass implements createChannels() to declare what g-buffer channel it
//    actually creates and writes to. Each one can be used in a later stage as a descriptor.
//  - the Stage subclass implements createNode() to assemble the actual rendering graph
//    that VSG will record for the stage.
//  - a Stage doesn't have to render; it could also record a Barrier or a Compute Dispatch.
//  - you are responsible for barriers and making sure channel/attachment indices line up
//  - you are resonsible for making sure descriptor bindings are correctly reflected in shaders.
//
// There are 2 example Stages here:
//  - RenderToGBuffer: renders the scene to a G-Buffer (albedo + normal + depth)
//  - RenderToFullScreenQuad: reads from the G-Buffer and renders a full-screen quad with some
//      single lighting and post-processing effects
//
// Notes:
//  - the stock VSG shaders don't support g-buffer outputs, so we copied them and added those
//    outputs. In the future it would be nice to include them in VSG proper and activate them
//    with a pragma import preprocessor define like VSG_GBUFFER (what we used here).
//
// TODOs:
//  - Clean up validation errors
//  - Resize the g-buffer when the user resizes the window (optionally)
//  - Consider a tighter format for the normal buffer, R8G8B8 is probably overkill
//  - Support more g-buffer channels like material, objectid, and position
//  - Implement more post-processing examples, like SSAO



// An Channel is a single g-buffer component. It may be used as an attachment
// when rendering to the g-buffer, or as a descriptor when reading from it later.
class Channel
{
public:
    std::string name;
    vsg::ref_ptr<vsg::ImageInfo> imageInfo;
    vsg::AttachmentDescription description;
    VkImageLayout layout;

    //! Creates a descriptor for this attachment, which will let you access it in a shader
    vsg::ref_ptr<vsg::Descriptor> createDescriptor(std::uint32_t binding) const
    {
        return vsg::DescriptorImage::create(imageInfo, binding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
};

// name-keyed channel dictionary
using Channels = std::unordered_map<std::string, Channel>;


// "ViewInfo" just lumps together information relevant to a particular vsg::View
// that will be rendered by a ViewWorkflow.
struct ViewInfo
{
    ViewInfo(vsg::Window* in_window, vsg::View* in_view, vsg::Options* in_options) :
        window(in_window), view(in_view), options(in_options) {
    }

    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::View> view;
    vsg::ref_ptr<vsg::Options> options;
};


// One step in a render workflow.
// This will usually be a rendergraph for drawing, a compute dispatch, or a barrier.
class Stage : public vsg::Inherit<vsg::Object, Stage>
{
public:
    std::string name;

    //! Creates a collection of channels that this stage will output. Optional.
    virtual std::vector<Channel> createChannels(ViewInfo& viewInfo) {
        return {};
    }

    //! Creates the node that will render this stage.
    virtual vsg::ref_ptr<vsg::Node> createNode(ViewInfo& viewInfo, const Channels& channels) = 0;
};


// A "chain" of work stages that assemble a renderable frame.
class ViewWorkflow : public vsg::Inherit<vsg::Group, ViewWorkflow>
{
public:
    ViewWorkflow(vsg::Window* in_window, vsg::View* in_view, vsg::Options* in_options) :
        viewInfo(in_window, in_view, in_options) {
    }
    
    ViewInfo viewInfo;
    std::vector<vsg::ref_ptr<Stage>> stages;
    
    Channels channels; // all attachments used in this workflow, keyed by name


    //! Generates the graph to render this view to the swapchain.
    void build()
    {
        vsg::Context cx(viewInfo.window->getOrCreateDevice());

        // collect the channels from each stage
        channels.clear();
        for (auto& stage : stages)
        {
            auto stageChannels = stage->createChannels(viewInfo);

            for (auto& a : stageChannels)
            {
                channels[a.name] = a;
            }
        }

        // build the actual node graph for each stage and add it
        for (auto& stage : stages)
        {
            auto node = stage->createNode(viewInfo, channels);
            if (node)
            {
                this->addChild(node);
            }
        }
    }
};

//! Creates a ShaderSet that we can pass to vsg::Builder to create a deferred rendering pipeline.
vsg::ref_ptr<vsg::ShaderSet>
createGBufferShaderSet(vsg::Options* options)
{
    vsg::ref_ptr<vsg::Options> refOptions(options);

    // the OSG flat shader (no changes)
    auto vertexShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_VERTEX_BIT, "main",
        vsg::findFile("deferred.standard.vert", refOptions),
        refOptions);

    // the OSG flat fragment shader adapted for deferred rendering
    auto fragmentShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_FRAGMENT_BIT, "main",
        vsg::findFile("deferred.standard.frag", refOptions),
        refOptions);

    if (!vertexShader || !fragmentShader)
        return { };

    auto shaderSet = vsg::ShaderSet::create(
        vsg::ShaderStages{ vertexShader, fragmentShader });

    // attrs in the "standard" shader.
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord0", "VSG_TEXTURECOORD_0", 2, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord1", "VSG_TEXTURECOORD_1", 3, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord2", "VSG_TEXTURECOORD_2", 4, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_TexCoord3", "VSG_TEXTURECOORD_3", 5, VK_FORMAT_R32G32_SFLOAT, {});
    shaderSet->addAttributeBinding("vsg_Color", "", 6, VK_FORMAT_R32G32B32A32_SFLOAT, {});

    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    //shaderSet->optionalDefines.emplace("VSG_GBUFFER");
    shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();
    shaderSet->defaultShaderHints->defines.emplace("VSG_GBUFFER");

    // Configure ColorBlendState for 2 color attachments (albedo + normal)
    // and assign it as the "default" state for a piepline.
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = {
        // albedo attachment
        {
            false,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT },
        // normal attachment
        {
            false,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
    };
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);
    shaderSet->defaultGraphicsPipelineStates.push_back(vsg::DepthStencilState::create());

    return shaderSet;
}



// NOTE: This is NOT USED in this demo BUT will be helpful later when rendering non-vsg models.
vsg::ref_ptr<vsg::Node> createGBufferPipeline(vsg::ShaderSet* shaderSet, vsg::ShaderCompileSettings* compileSettings)
{
    auto gc = vsg::GraphicsPipelineConfigurator::create(vsg::ref_ptr<vsg::ShaderSet>(shaderSet));

    gc->shaderHints = compileSettings ?
        vsg::ShaderCompileSettings::create(*compileSettings) :
        vsg::ShaderCompileSettings::create();

    gc->enableArray("vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gc->enableArray("vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, 12);
    gc->enableArray("vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, 8);

    // or whatever your ViewDependentData descriptors are called:
    gc->enableDescriptor("vsg_lights");
    gc->enableDescriptor("vsg_viewports");

    gc->init();

    auto commands = vsg::Commands::create();
    commands->addChild(gc->bindGraphicsPipeline);

    //commands->addChild(vsg::BindViewDescriptorSets::create(
    //    VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    gc->layout,
    //    VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX));

    return commands;
}



// then we can design various stages.

//! Workflow stage to render a scene to the G-Buffer.
class RenderToGBuffer : public vsg::Inherit<Stage, RenderToGBuffer>
{
    // creates all the outputs this stage will write to
    std::vector<Channel> createChannels(ViewInfo& viewInfo) override
    {
        std::vector<Channel> output;

        auto extent = viewInfo.view->camera->getRenderArea().extent;

        vsg::Context cx(viewInfo.window->getOrCreateDevice());

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
            description.format = image->format;
            description.samples = image->samples;
            description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            output.emplace_back(Channel{
                "albedo", 
                imageInfo,
                std::move(description),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        }

        // Normal attachment:
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

            auto imageInfo = vsg::ImageInfo::create();
            imageInfo->imageView = imageView;
            imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo->sampler = sampler;

            vsg::AttachmentDescription description;
            description.format = image->format;
            description.samples = image->samples;
            description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            output.emplace_back(Channel{
                "normal",
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
            description.format = image->format;
            description.samples = image->samples;
            description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            output.emplace_back(Channel{
                "depth",
                imageInfo,
                std::move(description),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });
        }

        return output;
    }

    // assembles the RenderGraph for g-buffer rendering
    vsg::ref_ptr<vsg::Node> createNode(ViewInfo& viewInfo, const Channels& channels) override
    { 
        // build RenderPass from attachments
        vsg::RenderPass::Attachments renderPassAttachments;
        vsg::RenderPass::Dependencies dependencies;
        vsg::RenderPass::Subpasses subpassDescriptions(1);
        vsg::ImageViews imageViews;

        vsg::Context cx(viewInfo.window->getOrCreateDevice());

        auto& albedo = channels.at("albedo");
        auto& normal = channels.at("normal");
        auto& depth = channels.at("depth");

        renderPassAttachments = {
            albedo.description,
            normal.description,
            depth.description
        };

        subpassDescriptions[0].colorAttachments = {
            vsg::AttachmentReference{ 0, albedo.layout },
            vsg::AttachmentReference{ 1, normal.layout }
        };

        subpassDescriptions[0].depthStencilAttachments = {
            vsg::AttachmentReference{ 2, depth.layout }
        };

        imageViews = {
            albedo.imageInfo->imageView,
            normal.imageInfo->imageView,
            depth.imageInfo->imageView
        };

        auto renderPass = vsg::RenderPass::create(cx.device, renderPassAttachments, subpassDescriptions, dependencies);

        // Framebuffer
        auto extent = viewInfo.view->camera->getRenderArea().extent;
        auto framebuffer = vsg::Framebuffer::create(renderPass, imageViews, extent.width, extent.height, 1);

        // Build the actual RenderGraph to render the scene to our framebuffer (instead of the swapchain)
        auto renderGraph = vsg::RenderGraph::create();
        renderGraph->framebuffer = framebuffer;
        renderGraph->renderArea.offset = VkOffset2D{ 0, 0 };
        renderGraph->renderArea.extent = extent;

        renderGraph->clearValues.resize(3);
        renderGraph->clearValues[0].color = vsg::vec4(0.3f, 0.05f, 0.1f, 1.0f); // as you like it
        renderGraph->clearValues[1].color = vsg::vec4(0.5f, 0.5f, 1.0f, 1.0f); // default normal = pointing at camera
        renderGraph->clearValues[2].depthStencil = VkClearDepthStencilValue{ 0.0f, 0 };

        // Set the render pass on the render graph so pipelines can be compiled against it
        renderGraph->renderPass = renderPass;

        renderGraph->addChild(viewInfo.view);
        return renderGraph;
    }
};


//! Workflow stage to read from the g-buffer and render to a full-screen quad
//! This is typically where you will apply lighting and optional post-processing
class RenderToFullScreenQuad : public vsg::Inherit<Stage, RenderToFullScreenQuad>
{
public:
    vsg::ref_ptr<vsg::Node> createNode(ViewInfo& viewInfo, const Channels& channels) override
    {
        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT, "main",
            vsg::findFile("deferred.fsq.vert", viewInfo.options),
            viewInfo.options);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT, "main",
            vsg::findFile("deferred.fsq.frag", viewInfo.options),
            viewInfo.options);

        if (!vertexShader || !fragmentShader)
            return { };

        auto vertexInputState = vsg::VertexInputState::create();

        // Tri-strip to render out full screen quad:
        auto inputAssemblyState = vsg::InputAssemblyState::create();
        inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        // No culling:
        auto rasterizationState = vsg::RasterizationState::create();
        rasterizationState->cullMode = VK_CULL_MODE_NONE;

        // No depth testing:
        auto depthStencilState = vsg::DepthStencilState::create();
        depthStencilState->depthTestEnable = VK_FALSE;

        auto viewportState = vsg::ViewportState::create(viewInfo.window->extent2D());

        auto multisampleState = vsg::MultisampleState::create();

        auto colorBlendState = vsg::ColorBlendState::create();
        colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
            { false,
              VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
              VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
              VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT }
        };

        auto pipelineLayout = vsg::PipelineLayout::create();

        // Bindings for each channel descriptor from the G-Buffer that we want to access.
        vsg::DescriptorSetLayoutBindings bindings{
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // albedo
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // normal
            { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}  // depth
        };

        auto dsetLayout = vsg::DescriptorSetLayout::create(bindings);
        pipelineLayout->setLayouts.emplace_back(dsetLayout);

        // Create our pipeline
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

        // And a command to bind it:
        auto bindPipeline = vsg::BindGraphicsPipeline::create(pipeline);


        // Create a descriptor set for our channel uniforms.
        auto dset = vsg::DescriptorSet::create(dsetLayout, vsg::Descriptors{
            channels.at("albedo").createDescriptor(0),
            channels.at("normal").createDescriptor(1),
            channels.at("depth").createDescriptor(2)
            });

        // ..and a command to bind it:
        auto bindDescriptors = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, dset);

        // draw the FSQ - just a single tristrip created in the shader
        auto drawFSQ = vsg::Draw::create(4, 1, 0, 0);

        // TODO: later when we have actual inset viewports, we might need
        // to create an actual vsg::View/Camera/Viewport combo.
        //auto view = vsg::View::create(vsg::Camera::create(), commands);
        //auto renderGraph = vsg::RenderGraph::create(channel->window, view);

        auto renderGraph = vsg::RenderGraph::create(viewInfo.window);
        renderGraph->addChild(bindPipeline);
        renderGraph->addChild(bindDescriptors);
        renderGraph->addChild(drawFSQ);

        return renderGraph;
    }
};



void fail(std::string_view msg)
{
    std::cout << msg << std::endl;
    exit(-1);
}


//! Loads up a simple scene for rendering
vsg::ref_ptr<vsg::Node> loadScene(vsg::Options* options)
{
    // You can configure the builder to use a custom shaderset. This lets us inject
    // our own G-buffer-capable shaders and pipeline states into the resulting model.
    vsg::Builder builder;
    builder.shaderSet = createGBufferShaderSet(options);
    if (!builder.shaderSet)
        fail("createGBufferShaderSet returned null shader set - make sure VSG_FILE_PATH points to the deferred shaders!");

    // Make a pretty cube
    vsg::GeometryInfo gi(vsg::box(vsg::vec3(-1, -1, -1), vsg::vec3(1, 1, 1)));
    gi.color = vsg::vec4(0.5f, 1.0f, 0.5f, 1.0f);
    vsg::StateInfo si;
    si.lighting = true, si.wireframe = false;
    auto scene = builder.createBox(gi, si);

    return scene;
}

//! Convenience function to make a camera that focuses on a node
vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* node, vsg::Window* window)
{
    vsg::ComputeBounds computeBounds;
    node->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    // set up the camera
    double nearFarRatio = 0.00001;
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

    // make a viewer with a window:
    auto windowTraits = vsg::WindowTraits::create(1920, 1080, argv[0]);
    windowTraits->instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (args.read("--api"))
        windowTraits->apiDumpLayer = true;
    if (args.read("--debug"))
        windowTraits->debugLayer = true;
    auto window = vsg::Window::create(windowTraits);
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load up a model to render:
    auto scene = loadScene(options);

    // and a camera to look at it:
    auto camera = createCameraForScene(scene, window);

    // and a view that uses this camera
    auto view = vsg::View::create(camera);

    view->addChild(scene);

    // we don't strictly need a light until our shaders do actual lighting
    // but for now let's keep it around
    view->addChild(vsg::createHeadlight());


    // build the workflow graph for our view:
    auto workflow = ViewWorkflow::create(window, view, options);

    auto renderToGBuffer = RenderToGBuffer::create();
    if (!renderToGBuffer)
        fail("RenderToGBuffer::create() returned null - check your VSG_FILE_PATH");
    workflow->stages.emplace_back(renderToGBuffer);

    auto renderToFSQ = RenderToFullScreenQuad::create();
    if (!renderToFSQ)
        fail("RenderToFullScreenQuad::create() returned null - check your VSG_FILE_PATH");
    workflow->stages.emplace_back(renderToFSQ);

    // builds the scene graph for each render stage
    workflow->build();

    // install the view workflow on the window's command graph:
    auto commandGraph = vsg::CommandGraph::create(window);
    commandGraph->children.emplace_back(workflow);


    // From here on out, normal VSG setup and frame loop.
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });
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
