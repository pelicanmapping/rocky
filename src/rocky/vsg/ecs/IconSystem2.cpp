
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IconSystem2.h"
#include "../VSGContext.h"
#include "../PipelineState.h"
#include "../Utils.h"
#include <rocky/Color.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/Draw.h>

using namespace ROCKY_NAMESPACE;

#define VERT_SHADER "shaders/rocky.icon.indirect.vert"
#define FRAG_SHADER "shaders/rocky.icon.indirect.frag"
#define CULL_SHADER "shaders/rocky.icon.indirect.cull.comp"

// these must match the layout() defs in the shaders.
#define BUFFER_SET 0  // must match layout(set=X) in the shader UBO

#define COMMAND_BUFFER_BINDING  0  // indirect command buffer
#define CULLLIST_BUFFER_BINDING 1  // input instance buffer
#define DRAWLIST_BUFFER_BINDING 2  // output drawlist buffer
#define SAMPLER_BINDING  3  // shared sampler uniform 
#define TEXTURES_BINDING 4  // array of textures uniform

#define COMMAND_BUFFER_NAME "command"
#define CULLLIST_BUFFER_NAME "cullList"
#define DRAWLIST_BUFFER_NAME "drawList"
#define SAMPLER_NAME "samp"
#define TEXTURES_NAME "textures"

#define MAX_CULL_LIST_SIZE 16384
#define GPU_CULLING_LOCAL_WG_SIZE 32 // TODO UP THIS TO 32 or 64
#define MAX_NUM_TEXTURES 1

namespace
{
    vsg::ref_ptr<vsg::ShaderSet> createRenderingShaderSet(VSGContext& context)
    {
        vsg::ref_ptr<vsg::ShaderSet> shaderSet;

        // load shaders
        auto vertexShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_VERTEX_BIT,
            "main",
            vsg::findFile(VERT_SHADER, context->searchPaths),
            context->readerWriterOptions);

        auto fragmentShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main",
            vsg::findFile(FRAG_SHADER, context->searchPaths),
            context->readerWriterOptions);

        if (!vertexShader || !fragmentShader)
        {
            return { };
        }

        vsg::ShaderStages shaderStages{ vertexShader, fragmentShader };

        shaderSet = vsg::ShaderSet::create(shaderStages);

        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });

        // bind the output draw list to both the compute stage (for writing) and
        // the vertex stage (for rendering)
        shaderSet->addDescriptorBinding(
            DRAWLIST_BUFFER_NAME, "",
            BUFFER_SET, DRAWLIST_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
            1, // count
            VK_SHADER_STAGE_VERTEX_BIT, {});

        // bind the array of textures to the fragment stage as a uniform:
        shaderSet->addDescriptorBinding(
            TEXTURES_NAME, "",
            BUFFER_SET, TEXTURES_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
            MAX_NUM_TEXTURES, 
            VK_SHADER_STAGE_FRAGMENT_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }

    vsg::ref_ptr<vsg::ShaderSet> createCullingShaderSet(VSGContext& context)
    {
        auto computeShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_COMPUTE_BIT,
            "main",
            vsg::findFile(CULL_SHADER, context->searchPaths),
            context->readerWriterOptions);

        if (!computeShader)
        {
            return {};
        }

        vsg::ShaderStages shaderStages{ computeShader };
        auto shaderSet = vsg::ShaderSet::create(shaderStages);

        // bind the indirect comand buffer to the compute stage:
        shaderSet->addDescriptorBinding(
            COMMAND_BUFFER_NAME, "",
            BUFFER_SET, COMMAND_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, {});

        // bind the input instance buffer to the compute stage:
        shaderSet->addDescriptorBinding(
            CULLLIST_BUFFER_NAME, "",
            BUFFER_SET, CULLLIST_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, {});

        // bind the output draw list to both the compute stage (for writing) and
        // the vertex stage (for rendering)
        shaderSet->addDescriptorBinding(
            DRAWLIST_BUFFER_NAME, "",
            BUFFER_SET, DRAWLIST_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, {});

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_COMPUTE_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        //shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_COMPUTE_BIT, 0, 128);

        return shaderSet;
    }

    vsg::ref_ptr<vsg::ImageInfo> makeDefaultImageInfo(IOOptions& io, vsg::ref_ptr<vsg::Sampler> sampler = {})
    {
        const char* icon_location = "https://readymap.org/readymap/filemanager/download/public/icons/airport.png";
        auto image = io.services.readImageFromURI(icon_location, io);
        auto imageData = util::moveImageToVSG(image.value);
        auto imageInfo = vsg::ImageInfo::create(sampler, imageData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return imageInfo;

        //const int d = 16;
        //auto image = Image::create(Image::R8G8B8A8_UNORM, d, d);
        //image->fill(Color::White);
        //for(int i=0; i<d; ++i)
        //{
        //    image->write(Color::Red, i, i);
        //    image->write(Color::Red, i, d - i - 1);
        //}

        //auto imageData = util::moveImageToVSG(image);
        //auto imageInfo = vsg::ImageInfo::create(sampler, imageData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        //return imageInfo;
    }
}

IconSystem2Node::IconSystem2Node(ecs::Registry& registry) :
    ecs::System(registry)
{
    //nop

    // TODO: move this
    auto [lock, r] = registry.write();

    r.on_construct<Icon>().template connect<&ecs::detail::SystemNode_on_construct<Icon>>();
    r.on_update<Icon>().template connect<&ecs::detail::SystemNode_on_update<Icon>>();
    r.on_destroy<Icon>().template connect<&ecs::detail::SystemNode_on_destroy<Icon>>();
}

IconSystem2Node::~IconSystem2Node()
{
    auto [lock, registry] = _registry.write();

    registry.on_construct<Icon>().template disconnect<&ecs::detail::SystemNode_on_construct<Icon>>();
    registry.on_update<Icon>().template disconnect<&ecs::detail::SystemNode_on_update<Icon>>();
    registry.on_destroy<Icon>().template disconnect<&ecs::detail::SystemNode_on_destroy<Icon>>();
}

void
IconSystem2Node::initializeSystem(VSGContext& context)
{
    // allocate all the buffers the compute shader will use.
    // the indirect command buffer
    drawCommandBuffer = vsg::ubyteArray::create(sizeof(VkDrawIndexedIndirectCommand));
    drawCommandBuffer->properties.dataVariance = vsg::DYNAMIC_DATA;

    // the cull list buffer
    cullBuffer = vsg::ubyteArray::create(sizeof(IconInstanceGPU) * MAX_CULL_LIST_SIZE);
    cullBuffer->properties.dataVariance = vsg::DYNAMIC_DATA;

    // create a shared sampler for our texture arena
    auto sampler = vsg::Sampler::create();
    sampler->maxLod = 5; // this alone will prompt mipmap generation!
    sampler->minFilter = VK_FILTER_LINEAR;
    sampler->magFilter = VK_FILTER_LINEAR;
    sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler->anisotropyEnable = VK_TRUE; // don't need this for a billboarded icon
    sampler->maxAnisotropy = 4.0f;

    // create the texture arena.
    textures.emplace_back(makeDefaultImageInfo(context->io, sampler));


    // Configure the compute pipeline for culling:
    auto cullShaderSet = createCullingShaderSet(context);
    if (!cullShaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Icon compute shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");

        return;
    }

    vsg::DescriptorSetLayoutBindings cull_descriptorBindings
    {
        {COMMAND_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {CULLLIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {DRAWLIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

    auto cull_descriptorSetLayout = vsg::DescriptorSetLayout::create(cull_descriptorBindings);

    auto cull_pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{
            cull_descriptorSetLayout, // set 0
        },
        vsg::PushConstantRanges{}); // no push constants

    auto cull_pipeline = vsg::ComputePipeline::create(cull_pipelineLayout, cullShaderSet->getShaderStages().front());
    auto cull_bindPipeline = vsg::BindComputePipeline::create(cull_pipeline);
    
    // the cull list is the input to the GPU culler.
    auto culllist_buffer_info = vsg::BufferInfo::create(cullBuffer);
    auto culllist_desciptor = vsg::DescriptorBuffer::create(
        vsg::BufferInfoList{ culllist_buffer_info }, CULLLIST_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // the draw list is the output of the GPU culler and the input to the renderer.
    // for the drawlist, only allocate memory on the GPU (don't need it on the CPU at all)
    auto drawlist_size = MAX_CULL_LIST_SIZE * sizeof(IconInstanceGPU);

    auto drawlist_buffer = vsg::createBufferAndMemory(
        context->device(),
        drawlist_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto drawlist_buffer_info = vsg::BufferInfo::create();
    drawlist_buffer_info->buffer = drawlist_buffer;
    drawlist_buffer_info->offset = 0;
    drawlist_buffer_info->range = drawlist_size;

    auto drawlist_desciptor = vsg::DescriptorBuffer::create(
        vsg::BufferInfoList{ drawlist_buffer_info }, DRAWLIST_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);



    // manually create the draw commands buffer so we can use it for indirect commands.
    auto indirect_command_buffer_info = vsg::BufferInfo::create(drawCommandBuffer);

    auto indirect_command_descriptor = DescriptorBufferEx::create(
        vsg::BufferInfoList{ indirect_command_buffer_info }, COMMAND_BUFFER_BINDING, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    // bind all our descriptors to the pipeline.
    auto cull_bindDescriptors = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        cull_pipelineLayout,
        vsg::DescriptorSet::create(
            cull_descriptorSetLayout,
            vsg::Descriptors{
                indirect_command_descriptor, culllist_desciptor, drawlist_desciptor }));


#if 0
    auto indirectCommandBufferBarrier = vsg::BufferMemoryBarrier::create(
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        indirect_command_buffer_info->buffer,
        0,
        VK_WHOLE_SIZE);

    auto cull_barrierCommand = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        indirectCommandBufferBarrier);
#endif

    //auto cull_zeroDrawList = ZeroBuffer::create(drawlist_buffer);

    // stick it all under the compute graph.
    auto cg = context->getComputeCommandGraph();
    //cg->addChild(cull_barrierCommand);
    //cg->addChild(cull_zeroDrawList);
    cg->addChild(cull_bindPipeline);
    cg->addChild(cull_bindDescriptors);

    // This brings in the VSG set=1 bing commands (vsg_viewport, vsg_lights)
    //for (auto& cdsb : cullShaderSet->customDescriptorSetBindings)
    //    this->addChild(cdsb->createStateCommand(cull_pipelineLayout));

    cg->addChild(cullDispatch = vsg::Dispatch::create(1, 1, 1));





    // now configure the rendering pipeline.
    auto renderShaderSet = createRenderingShaderSet(context);
    if (!renderShaderSet)
    {
        status = Status(Status::ResourceUnavailable,
            "Icon shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    vsg::VertexInputState::Bindings draw_vertexBindings
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}  // "in_vertex"
    };

    vsg::VertexInputState::Attributes draw_vertexAttributes
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}  // "in_vertex"
    };

    // define the draw pipeline template.
    vsg::DescriptorSetLayoutBindings draw_descriptorBindings
    {
        {COMMAND_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {DRAWLIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_NUM_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    vsg::PushConstantRanges draw_pushConstantRanges
    {
        // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
    };

    auto inputAssemblyState = vsg::InputAssemblyState::create();
    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;

    auto depthStencilState = vsg::DepthStencilState::create();
    //depthStencilState->depthCompareOp = VK_COMPARE_OP_ALWAYS;
    //depthStencilState->depthTestEnable = VK_FALSE;
    //depthStencilState->depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend;
    blend.blendEnable = true;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{ blend };
    
    vsg::GraphicsPipelineStates draw_pipelineStates
    {
        vsg::VertexInputState::create(draw_vertexBindings, draw_vertexAttributes),
        inputAssemblyState,
        rasterizationState,
        vsg::MultisampleState::create(),
        colorBlendState,
        depthStencilState
    };

    auto draw_descriptorSetLayout = vsg::DescriptorSetLayout::create(draw_descriptorBindings);

    auto draw_pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts {
            draw_descriptorSetLayout, // set 0
            PipelineUtils::getViewDependentDescriptorSetLayout() // set 1 (vsg_viewport, vsg_lights)
        },
        draw_pushConstantRanges);

    auto draw_pipeline = vsg::GraphicsPipeline::create(draw_pipelineLayout, renderShaderSet->getShaderStages(), draw_pipelineStates);
    auto draw_bindPipeline = vsg::BindGraphicsPipeline::create(draw_pipeline);

    auto draw_textures_descriptor = vsg::DescriptorImage::create(
        vsg::ImageInfoList{ textures },
        TEXTURES_BINDING, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto draw_bindDescriptorSet = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        draw_pipelineLayout,
        BUFFER_SET,
        vsg::DescriptorSet::create(
            draw_descriptorSetLayout,
            vsg::Descriptors{
                drawlist_desciptor, draw_textures_descriptor }));

    // Add our binders to the scene graph.
    this->addChild(draw_bindPipeline);
    this->addChild(draw_bindDescriptorSet);

    // This brings in the VSG set=1 bing commands (vsg_viewport, vsg_lights)
    for (auto& cdsb : renderShaderSet->customDescriptorSetBindings)
    {
        this->addChild(cdsb->createStateCommand(draw_pipelineLayout));
    }

#if 0
    auto indirectCommandBufferBarrier = vsg::BufferMemoryBarrier::create(
        0,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        indirect_command_buffer_info->buffer,
        0,
        VK_WHOLE_SIZE);

    this->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0,
        indirectCommandBufferBarrier));
#endif


    // the actual rendering command, finally.
    auto draw = vsg::DrawIndexedIndirect::create();
    draw->bufferInfo = indirect_command_buffer_info;
    draw->drawCount = 1;
    draw->stride = 0;

    // billboard geometry (with dummy vertex positions; shader will generate them)
    auto geometry = vsg::Geometry::create();
    geometry->assignIndices(vsg::ushortArray::create({ 0, 1, 2, 2, 3, 0 }));
    geometry->assignArrays(vsg::DataList{ vsg::vec3Array::create(4) });
    geometry->commands.emplace_back(draw);

    this->addChild(geometry);
}

namespace
{
    template<typename CACHE, typename MUTEX, typename CREATE>
    vsg::ref_ptr<vsg::DescriptorImage>& getOrCreate(CACHE& cache, MUTEX& mutex, std::shared_ptr<Image> key, CREATE&& create)
    {
        std::scoped_lock L(mutex);
        auto iter = cache.find(key);
        if (iter != cache.end())
            return iter->second;
        else
            return cache[key] = create();
    }
}

void
IconSystem2Node::update(VSGContext& context)
{
    if (!status.ok())
        return;

    // reset the indirect command buffer
    auto& cmd = *static_cast<VkDrawIndexedIndirectCommand*>(drawCommandBuffer->dataPointer());
    cmd = VkDrawIndexedIndirectCommand{ 6, 0, 0, 0, 0 };

    // update the cull list
    auto* cullList = static_cast<IconInstanceGPU*>(cullBuffer->dataPointer());
    
    int count = 0;

    auto [lock, registry] = _registry.read();

    // TODO: do this for each active view!

    registry.view<Icon, ActiveState, Visibility, Transform>().each([&](
        auto& icon, auto& active, auto& visibility, auto& transform)
        {
            if (ecs::visible(visibility, 0)) // && !transform.node->viewLocal[0].culled)
            {
                auto& viewLocal = transform.node->viewLocal[0];

                auto& instance = cullList[count++];
                instance.proj = viewLocal.proj;
                instance.modelview = viewLocal.modelview;
                instance.viewport = viewLocal.viewport;
                instance.size = icon.style.size_pixels;
                instance.rotation = icon.style.rotation_radians;
                instance.texture_index = 0;
            }
        });


    // configure the culling shader for 'count' instances
    unsigned workgroups = (count + (GPU_CULLING_LOCAL_WG_SIZE - 1)) / GPU_CULLING_LOCAL_WG_SIZE;
    cullDispatch->groupCountX = workgroups;

    // zero from the end of the cull list to the padding boundary;
    // this will set the "radius" entries to zero, indicating a blank/padding entry
    unsigned padding = (workgroups * GPU_CULLING_LOCAL_WG_SIZE) - count;
    if (padding > 0)
    {
        std::memset(&cullList[count], 0, padding * sizeof(IconInstanceGPU));
    }

    drawCommandBuffer->dirty();

    // We really only need to re-transfer the bit we changed, but
    // VSG does not yet support partial transfer ranges.
    cullBuffer->dirty();
}

void
IconSystem2Node::traverse(vsg::RecordTraversal& rt) const
{    
    // now run the actual render commands
    Inherit::traverse(rt);

    auto [lock, registry] = _registry.read();

    registry.view<Icon, ActiveState, Transform>().each([&](
        const entt::entity entity, auto& icon, auto& active, auto& transform)
        {
            // update the transform (but don't do any culling)
            transform.push(rt, false);
        });
}
