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

    _shader->module->hints = vsgcontext->shaderCompileSettings;
}

void
FrustumGridSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

    for(auto& view : _views)
    {
        if (view.newGrid.has_value())
        {
            // wait for any previous work to finish before we resize the buffers
            vsgcontext->viewer()->deviceWaitIdle();

            auto& grid = view.newGrid.value();
            auto& vds = _sharedRenderData->viewDependentState[grid.viewID];

            Log()->debug("FrustumGridSystem: updating cluster grid for view {} to {}x{}", 
                grid.viewID, grid.viewport[2], grid.viewport[3]);

            // Start by updating the paramters uniform with the new values:
            BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
            params->invProjMatrix = to_glm(vsg::inverse(grid.projection));
            params->viewport = glm::ivec4(grid.viewport[0], grid.viewport[1], grid.viewport[2], grid.viewport[3]);
            params->numTiles = glm::uvec2(
                (grid.viewport[2] + FRUSTUM_GRID_TILE_SIZE - 1u) / FRUSTUM_GRID_TILE_SIZE,
                (grid.viewport[3] + FRUSTUM_GRID_TILE_SIZE - 1u) / FRUSTUM_GRID_TILE_SIZE);
            params->pixelsPerTile = FRUSTUM_GRID_TILE_SIZE; // assuming square tiles
            requestUpload(params);

            // Allocate space on the GPU for all the actual frustums and update the buffer.
            // This will require a recompile of course.
            Log()->info("FrustumSystemNode: reallocating {} tiles", params->numTiles.x * params->numTiles.y);
            GPUOnlyBufferAccess<FrustumGPU> frustums(vds->frustumsBuf, vsgcontext->device());
            auto old = frustums.resize(params->numTiles.x * params->numTiles.y);
            dispose(old);

            // reallocating buffers requires rebuilding the descriptor sets that hold them.
            vds->recompileDescriptorSets();

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

            view.newGrid.reset();
        }
    }

    Inherit::update(vsgcontext);
}

void
FrustumGridSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed()) return;

    auto* state = record.getState();
    ROCKY_SOFT_ASSERT_AND_RETURN(state && state->_commandBuffer, void());

    // Is there a better way to detect a compute traversal?
    bool isCompute = _lastFrameCount != record.getFrameStamp()->frameCount;
    _lastFrameCount = record.getFrameStamp()->frameCount;

    //bool isCompute = !state->_commandBuffer->viewDependentState;
    if (isCompute)
    {
        for (unsigned viewID=0; viewID <_views.size(); ++viewID)
        {
            auto& view = _views[viewID];
            if (view.commands)
            {
                if (_sharedRenderData->viewDependentState[viewID])
                {
                    // active view; dispatch compute shader
                    view.commands->accept(record);
                }
                else
                {
                    // detect a removed view and dispose of its contents
                    dispose(view.commands);
                    view.commands = {};
                    view.newGrid.reset();
                }
            }
        }
    }

    else // rendering traversal:
    {
        auto viewID = state->_commandBuffer->viewID;
        auto vds = _sharedRenderData->viewDependentState[viewID];
        auto& view = _views[viewID];

        //ROCKY_SOFT_ASSERT_AND_RETURN(vds, void());
        if (!vds) {
            // view has probably been destroyed but traversals are still in progress..why?
            return;
        }

        // Extract the viewport size:
        auto* viewportData = state->_commandBuffer->viewDependentState->viewportData.get();
        if (!viewportData || viewportData->empty())
            return;

        const auto& vp = (*viewportData)[0];

        auto& projMatrix = state->projectionMatrixStack.top();

        BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);

        if (params->viewport[2] != vp[2] || params->viewport[3] != vp[3] || projMatrix[3][3] != view.lastProjMatrix[3][3])
        {
            // If the viewport size changes we have to reallocate the grid.
            // Queue an update for the next frame.
            Grid newGrid;
            newGrid.viewID = viewID;
            newGrid.viewport = vp;
            newGrid.projection = projMatrix;
            view.newGrid = std::move(newGrid);
            view.lastProjMatrix = projMatrix;
        }
        else if (params->viewport[0] != vp[0] || params->viewport[1] != vp[1])
        {
            // viewport offset changed, update the params but don't reallocate the grid.
            // There will be a one frame delay.
            // TODO: consider moving this to update?
            params->viewport = glm::ivec4(vp[0], vp[1], vp[2], vp[3]);
            requestUpload(params);
        }
    }
}
