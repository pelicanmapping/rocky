
/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "IconSystem2.h"
#include "../VSGContext.h"
#include "../PipelineState.h"
#include "../VSGUtils.h"
#include <rocky/Color.h>

#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/commands/Draw.h>

using namespace ROCKY_NAMESPACE;

#define VERT_SHADER "shaders/rocky.icon.indirect.vert"
#define FRAG_SHADER "shaders/rocky.icon.indirect.frag"
#define CULL_SHADER "shaders/rocky.icon.indirect.cull.comp"

// these must match the layout() defs in the shaders.
#define DESCRIPTOR_SET_INDEX 0  // must match layout(set=X) in the shader UBO

#define INDIRECT_COMMAND_BUFFER_BINDING  0  // indirect command buffer
#define CULL_LIST_BUFFER_BINDING 1  // input instance buffer
#define DRAW_LIST_BUFFER_BINDING 2  // output draw_list buffer
#define SAMPLER_BINDING  3  // shared sampler uniform 
#define TEXTURES_BINDING 4  // array of textures uniform

#define INDIRECT_COMMAND_BUFFER_NAME "command"
#define CULL_LIST_BUFFER_NAME "cull_list"
#define DRAW_LIST_BUFFER_NAME "draw_list"
#define SAMPLER_NAME "samp"
#define TEXTURES_NAME "textures"

#define MAX_CULL_LIST_SIZE 16384
#define GPU_CULLING_LOCAL_WG_SIZE 32 // TODO UP THIS TO 32 or 64
#define MAX_NUM_TEXTURES 1

namespace
{
    //! Create a shader set for the culling compute shader.
    vsg::ref_ptr<vsg::ShaderStage> createCullingShader(VSGContext& context)
    {
        auto computeShader = vsg::ShaderStage::read(
            VK_SHADER_STAGE_COMPUTE_BIT,
            "main",
            vsg::findFile(CULL_SHADER, context->searchPaths),
            context->readerWriterOptions);

        if (computeShader)
        {
            // Specializations to pass to the shader
            computeShader->specializationConstants = vsg::ShaderStage::SpecializationConstants{
                {0, vsg::intValue::create(GPU_CULLING_LOCAL_WG_SIZE)} }; // layout(load_size_x_id, 0) in
        }

        return computeShader;
    }


    //! Load and configure the shader stages for rendering.
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

        shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertexShader, fragmentShader });

#if 0
        // "binding" (3rd param) must match "layout(location=X) in" in the vertex shader
        shaderSet->addAttributeBinding("in_vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, { });

        // bind the output draw list to both the compute stage (for writing) and
        // the vertex stage (for rendering)
        shaderSet->addDescriptorBinding(
            DRAW_LIST_BUFFER_NAME, "",
            DESCRIPTOR_SET_INDEX,
            DRAW_LIST_BUFFER_BINDING,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1, // count
            VK_SHADER_STAGE_VERTEX_BIT, {});

        // bind the array of textures to the fragment stage as a uniform:
        shaderSet->addDescriptorBinding(
            TEXTURES_NAME, "",
            DESCRIPTOR_SET_INDEX,
            TEXTURES_BINDING,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            MAX_NUM_TEXTURES,
            VK_SHADER_STAGE_FRAGMENT_BIT, {});
#endif

        // We need VSG's view-dependent data:
        PipelineUtils::addViewDependentData(shaderSet, VK_SHADER_STAGE_VERTEX_BIT);

        // Note: 128 is the maximum size required by the Vulkan spec so don't increase it
        shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

        return shaderSet;
    }


    vsg::ref_ptr<vsg::ImageInfo> makeDefaultImageInfo(IOOptions& io, vsg::ref_ptr<vsg::Sampler> sampler = {})
    {
#if 1
        const char* icon_location = "https://readymap.org/readymap/filemanager/download/public/icons/airport.png";
        auto image = io.services.readImageFromURI(icon_location, io);
        if (image.ok())
        {
            auto imageData = util::moveImageToVSG(image.value());
            auto imageInfo = vsg::ImageInfo::create(sampler, imageData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            return imageInfo;
        }
        else
        {
            return {};
        }
#else
        const int d = 16;
        auto image = Image::create(Image::R8G8B8A8_UNORM, d, d);
        image->fill(Color(0,0,0,0));
        for(int i=0; i<d; ++i)
        {
            image->write(Color::Red, i, i);
            image->write(Color::Red, i, d - i - 1);
        }

        auto imageData = util::moveImageToVSG(image);
        return vsg::ImageInfo::create(sampler, imageData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
    }
}

IconSystem2Node::IconSystem2Node(Registry& registry) :
    System(registry)
{
    //nop

    // TODO: move this
    auto [lock, r] = registry.write();

    r.on_construct<Icon>().template connect<&detail::SystemNode_on_construct<Icon>>();
    r.on_update<Icon>().template connect<&detail::SystemNode_on_update<Icon>>();
    r.on_destroy<Icon>().template connect<&detail::SystemNode_on_destroy<Icon>>();
}

IconSystem2Node::~IconSystem2Node()
{
    auto [lock, registry] = _registry.write();

    registry.on_construct<Icon>().template disconnect<&detail::SystemNode_on_construct<Icon>>();
    registry.on_update<Icon>().template disconnect<&detail::SystemNode_on_update<Icon>>();
    registry.on_destroy<Icon>().template disconnect<&detail::SystemNode_on_destroy<Icon>>();
}

void
IconSystem2Node::initialize(VSGContext& context)
{
    // a dynamic SSBO that holds the draw-indirect command. The compute shader will write to this
    // and the rendering shader will read from it.
    indirect_command = StreamingGPUBuffer::create(
        INDIRECT_COMMAND_BUFFER_BINDING,
        sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    // a dynamic SSBO that holds the list of instances to cull. The CPU populates it, the compute
    // shader reads from it.
    cull_list = StreamingGPUBuffer::create(
        CULL_LIST_BUFFER_BINDING,
        sizeof(IconInstanceGPU) * MAX_CULL_LIST_SIZE,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // A shared sampler for our texture arena
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

    // Place a default texture in the arena for now.
    textures.emplace_back(makeDefaultImageInfo(context->io, sampler));

    buildCullStage(context);

    buildRenderStage(context);
}

void
IconSystem2Node::buildCullStage(VSGContext& context)
{
    // Configure the compute pipeline for culling:
    auto compute_shader = createCullingShader(context);
    if (!compute_shader)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Icon compute shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");

        return;
    }

    vsg::DescriptorSetLayoutBindings descriptor_bindings
    {
        {INDIRECT_COMMAND_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {CULL_LIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {DRAW_LIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);

    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{ descriptor_set_layout }, // set 0
        vsg::PushConstantRanges{}); // no push constants

    // the pipeline itself, and its binder:
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_shader);
    auto bind_pipeline = vsg::BindComputePipeline::create(pipeline);

    // the draw list is the output of the GPU culler and the input to the renderer.
    // for the draw_list, only allocate memory on the GPU (don't need it on the CPU at all)
    auto draw_list_size = MAX_CULL_LIST_SIZE * sizeof(IconInstanceGPU);

    // GPU-only SSBO that will hold the final draw list.
    auto draw_list_buffer_info = vsg::BufferInfo::create(
        vsg::createBufferAndMemory(
            context->device(),
            draw_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // visible to GPU only!
        0,
        draw_list_size);

    draw_list_descriptor = vsg::DescriptorBuffer::create(
        vsg::BufferInfoList{ draw_list_buffer_info },
        DRAW_LIST_BUFFER_BINDING,
        0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);


    // bind all our descriptors to the pipeline.
    auto bind_descriptors = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_layout,
        vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{
                indirect_command->descriptor, cull_list->descriptor, draw_list_descriptor }));


    // stick it all under the compute graph.
    auto cg = context->getComputeCommandGraph();

    cg->addChild(indirect_command);
    cg->addChild(cull_list);
    cg->addChild(bind_pipeline);
    cg->addChild(bind_descriptors);
    cg->addChild(cull_dispatch = vsg::Dispatch::create(0, 1, 1)); // will be updated later
}

void
IconSystem2Node::buildRenderStage(VSGContext& context)
{
    auto shader_set = createRenderingShaderSet(context);
    if (!shader_set)
    {
        status = Failure(Failure::ResourceUnavailable,
            "Icon shaders are missing or corrupt. "
            "Did you set ROCKY_FILE_PATH to point at the rocky share folder?");
        return;
    }

    vsg::VertexInputState::Bindings vertex_bindings
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}  // "in_vertex"
    };

    vsg::VertexInputState::Attributes vertex_attributes
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}  // "in_vertex"
    };

    // Define the draw pipeline template.
    vsg::DescriptorSetLayoutBindings descriptor_bindings
    {
        {INDIRECT_COMMAND_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {DRAW_LIST_BUFFER_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_NUM_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    // PC's hold the projection and modelview matrices from VSG.
    vsg::PushConstantRanges push_constant_ranges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
    };

    // Assemble all the pipeline states:
    auto ia_state = vsg::InputAssemblyState::create();
    ia_state->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    auto rasterization_state = vsg::RasterizationState::create();
    rasterization_state->cullMode = VK_CULL_MODE_NONE;

    auto depth_stencil_state = vsg::DepthStencilState::create();
    //depth_stencil_state->depthCompareOp = VK_COMPARE_OP_ALWAYS;
    //depth_stencil_state->depthTestEnable = VK_FALSE;
    //depth_stencil_state->depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend;
    blend.blendEnable = true;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    auto color_blend_state = vsg::ColorBlendState::create();
    color_blend_state->attachments = vsg::ColorBlendState::ColorBlendAttachments{ blend };
    
    vsg::GraphicsPipelineStates pipeline_states
    {
        vsg::VertexInputState::create(vertex_bindings, vertex_attributes),
        ia_state,
        rasterization_state,
        vsg::MultisampleState::create(),
        color_blend_state,
        depth_stencil_state
    };

    // our layout:
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);

    // VSG's view-dependent stuff:
    auto view_dependent_binding = vsg::ViewDependentStateBinding::create(VSG_VIEW_DEPENDENT_DESCRIPTOR_SET_INDEX);
    auto view_dependent_descriptor_set_layout = view_dependent_binding->createDescriptorSetLayout();

    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts {
            descriptor_set_layout, // set 0
            view_dependent_descriptor_set_layout,    // set 1 (vsg_viewport, vsg_lights, etc)
        },
        push_constant_ranges);

    auto pipeline = vsg::GraphicsPipeline::create(pipeline_layout, shader_set->getShaderStages(), pipeline_states);
    auto bind_pipeline = vsg::BindGraphicsPipeline::create(pipeline);

    auto textures_descriptor = vsg::DescriptorImage::create(
        vsg::ImageInfoList{ textures },
        TEXTURES_BINDING, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto bind_descriptor_sets = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout,
        DESCRIPTOR_SET_INDEX,
        vsg::DescriptorSet::create(
            descriptor_set_layout,
            vsg::Descriptors{ draw_list_descriptor, textures_descriptor }));

    auto bind_view_dependent_descriptor_sets = view_dependent_binding->createStateCommand(pipeline_layout);

    // Add our binders to the scene graph.
    this->addChild(bind_pipeline);
    this->addChild(bind_descriptor_sets);
    this->addChild(bind_view_dependent_descriptor_sets);

    //// This brings in the VSG set=1 bing commands (vsg_viewport, vsg_lights)
    //for (auto& cdsb : shader_set->customDescriptorSetBindings)
    //{
    //    this->addChild(cdsb->createStateCommand(pipeline_layout));
    //}

#if 0
    auto indirect_command_barrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        indirect_command_buffer_info_list[0]->buffer,
        0,
        VK_WHOLE_SIZE);

    auto draw_list_barrier = vsg::BufferMemoryBarrier::create(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        draw_list_buffer_info->buffer,
        0,
        VK_WHOLE_SIZE);

    auto draw_apply_barriers = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0,
        indirect_command_barrier, draw_list_barrier);

    this->addChild(draw_apply_barriers);
#endif

    // the actual rendering command, finally.
    auto draw = vsg::DrawIndexedIndirect::create();
    draw->bufferInfo = indirect_command->ssbo; // _info;
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

    if (!context->renderingEnabled)
        return;

    // reset the indirect command buffer
    auto& cmd = indirect_command->data<VkDrawIndexedIndirectCommand>()[0];
    cmd = VkDrawIndexedIndirectCommand{ 6, 0, 0, 0, 0 };
    indirect_command->dirty();

    // update the cull list
    auto* instances = cull_list->data<IconInstanceGPU>();

    int count = 0;

    auto [lock, registry] = _registry.read();

    // This will built a draw list that applies to all active views.

    // TODO: Support ALL active views!
    auto view = registry.view<Icon, ActiveState, Visibility, TransformDetail>();

    view.each([&](auto& icon, auto& active, auto& visibility, auto& transform_detail)
        {
            for (auto viewID : context->activeViewIDs)
            {
                if (visible(visibility, viewID))
                {
                    auto& view = transform_detail.views[viewID];

                    auto& instance = instances[count++];
                    instance.proj = view.proj;
                    instance.modelview = view.modelview;
                    instance.viewport = view.viewport;
                    instance.size = icon.style.size_pixels;
                    instance.rotation = icon.style.rotation_radians;
                    instance.texture_index = 0;
                }
            }
        });


    // configure the culling shader for 'count' instances
    unsigned workgroups = (count + (GPU_CULLING_LOCAL_WG_SIZE - 1)) / GPU_CULLING_LOCAL_WG_SIZE;
    cull_dispatch->groupCountX = workgroups;

    // zero from the end of the cull list to the padding boundary;
    // this will set the "radius" entries to zero, indicating a blank/padding entry
    std::size_t padding_count = (workgroups * GPU_CULLING_LOCAL_WG_SIZE) - count;
    
    if (padding_count > 0)
    {
        auto offset = count * sizeof(IconInstanceGPU);
        auto bytes = std::min(padding_count * sizeof(IconInstanceGPU), MAX_CULL_LIST_SIZE - offset);
        std::memset(&instances[count], 0, bytes);
    }

    auto total_cull_list_bytes_to_dirty = (count + padding_count) * sizeof(IconInstanceGPU);
    cull_list->dirty(0, total_cull_list_bytes_to_dirty);        
}
