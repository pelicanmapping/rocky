/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "FrustumGridSystem.h"
#include "ShaderDefines.h"
#include <algorithm>
#include <cstdint>

using namespace ROCKY_NAMESPACE;

namespace
{
    constexpr const char* FRUSTUM_GRID_COMP_SHADER = "shaders/rocky.frustumgrid.computer.comp";

    // only used to reflect the frustum structure on the GPU side
    struct FrustumGPU
    {
        glm::fvec4 planes[4];
    };

    // Size of each square cluster in pixels
    constexpr std::uint32_t FRUSTUM_GRID_TILE_SIZE = 16;

    constexpr std::uint32_t FIXED_FRUSTUM_GRID_TILES_PER_DIMENSION = 128;
}

FrustumGridSystemNode::FrustumGridSystemNode(Registry& r) :
    Inherit(r)
{
}

void
FrustumGridSystemNode::initialize(VSGContext vsgcontext)
{
    if (!vsgcontext)
    {
        status = Failure(Failure::AssertionFailure, "FrustumGridSystem: invalid VSG context");
        return;
    }

    _sharedRenderData = vsgcontext->sharedRenderData;

    _shader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_COMPUTE_BIT,
        "main",
        vsg::findFile(FRUSTUM_GRID_COMP_SHADER, vsgcontext->searchPaths),
        vsgcontext->readerWriterOptions);

    if (!_shader)
    {
        status = Failure(Failure::ResourceUnavailable, "FrustumGridSystem: missing compute shader");
        return;
    }
}

void
FrustumGridSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

    for(auto& view : _views)
    {
        if (view.newViewport.has_value())
        {
            vsgcontext->viewer()->deviceWaitIdle(); // wait for any previous work to finish before we resize the buffers

            auto& vp = view.newViewport.value();
            auto& vds = _sharedRenderData->viewDependentState[vp.viewID];

            Log()->info("FrustumGridSystem: updating cluster grid for view {} to {}x{}", 
                vp.viewID, vp.width, vp.height);

            // Start by updating the paramters uniform with the new values:
            BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
            params->invProjMatrix = to_glm(vsg::inverse(vp.projection));
            params->viewport = glm::ivec4(vp.x, vp.y, vp.width, vp.height);
            params->numTiles = glm::uvec2(
                (vp.width + FRUSTUM_GRID_TILE_SIZE - 1u) / FRUSTUM_GRID_TILE_SIZE,
                (vp.height + FRUSTUM_GRID_TILE_SIZE - 1u) / FRUSTUM_GRID_TILE_SIZE);
            params->pixelsPerTile = FRUSTUM_GRID_TILE_SIZE; // assuming square tiles
            requestUpload(params);

            // Allocate space on the GPU for all the actual frustums and update the buffer.
            // This will require a recompile of course.
            GPUOnlyBufferAccess<FrustumGPU> frustums(vds->frustumsBuf, vsgcontext->device());
            auto old = frustums.resize(params->numTiles.x * params->numTiles.y);
            dispose(old);

            // manually release and recompile the descriptor holding our SSBO
            // so that it will point to the new buffer:
            vds->descriptorSet->release();
            vds->descriptorSet->compile(vsg::Context(vsgcontext->device()));

            // first time through, build the command dispatcher
            if (!view.commands)
            {
                // use the ds layout from the first view-dependent state, which is shared by all views:
                auto descriptorSetLayout = _sharedRenderData->viewDependentState[0]->descriptorSetLayout;

                auto pipelineLayout = vsg::PipelineLayout::create(
                    vsg::DescriptorSetLayouts{
                        vsg::DescriptorSetLayout::create(), // note: layout 0 is empty
                        descriptorSetLayout },
                        vsg::PushConstantRanges{});

                auto pipeline = vsg::ComputePipeline::create(pipelineLayout, _shader);

                auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

                // binds the DS to the compute shader:
                auto bindDescriptorSet = vsg::BindDescriptorSet::create(
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->layout,
                    VDS_DESCRIPTOR_SET_INDEX,
                    vds->descriptorSet);

                // launches the compute shader:
                auto dispatch = vsg::Dispatch::create(
                    (params->numTiles.x + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
                    (params->numTiles.y + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
                    1u);

                // and a barrier to ensure the frustums are written before any subsequent rendering:
                auto bufferBarrier = vsg::BufferMemoryBarrier::create(
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    vds->frustumsBuf->bufferInfoList[0]->buffer,
                    0, VK_WHOLE_SIZE);

                auto barrier = vsg::PipelineBarrier::create(
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0, // dependency flags
                    bufferBarrier);

                view.commands = vsg::Commands::create();
                view.commands->addChild(bindPipeline);
                view.commands->addChild(bindDescriptorSet);
                view.commands->addChild(dispatch);
                view.commands->addChild(barrier);
            }

            requestCompile(view.commands);

            view.newViewport.reset();
        }
    }

    Inherit::update(vsgcontext);
}

void
FrustumGridSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    auto* state = record.getState();
    //ROCKY_SOFT_ASSERT_AND_RETURN(_bindPipeline, void());
    ROCKY_SOFT_ASSERT_AND_RETURN(state && state->_commandBuffer, void());

    bool isCompute = !state->_commandBuffer->viewDependentState;

    if (isCompute)
    {
        for (auto& view : _views)
        {
            if (view.commands)
                view.commands->accept(record);
        }
    }

    else // isRender
    {
        auto viewID = state->_commandBuffer->viewID;
        auto vds = _sharedRenderData->viewDependentState[viewID];

        // Extract the viewport size:
        auto* viewportData = state->_commandBuffer->viewDependentState->viewportData.get();
        if (!viewportData || viewportData->empty())
            return;

        const auto& vp = (*viewportData)[0];
        auto width = std::max(0u, (std::uint32_t)vp[2]);
        auto height = std::max(0u, (std::uint32_t)vp[3]);
        ROCKY_SOFT_ASSERT_AND_RETURN(width > 0u && height > 0u, void());

        auto& view = _views[viewID];

        // Access the shared data describing the frustum grid for this view,
        // creating it if necessary.
        BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);

        // If the viewport size changes we have to reallocate the grid.
        const bool changed = params->viewport[2] != width || params->viewport[3] != height;
        if (changed)
        {
            // queue an update for the next frame.
            Viewport vp;
            vp.viewID = viewID;
            vp.x = params->viewport[0], vp.y = params->viewport[1];
            vp.width = width, vp.height = height;
            vp.projection = state->projectionMatrixStack.top();
            view.newViewport = std::move(vp);
        }
    }
}
