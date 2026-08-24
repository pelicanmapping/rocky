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

    _sharedRenderData = vsgcontext->sharedRenderData;
}

void
FrustumGridSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed()) return;

    for(size_t viewID = 0; viewID < _views.size(); ++viewID)
    {
        auto& view = _views[viewID];
        auto& vds = _sharedRenderData->viewDependentState[viewID];
        if (vds)
        {
            if (view.newGrid.has_value())
            {
                // wait for any previous work to finish before we resize the buffers
                vsgcontext->viewer()->deviceWaitIdle();

                auto& grid = view.newGrid.value();

                // Start by updating the paramters uniform with the new values:
                BufferAccess<FrustumGridParamsGPU> params(vds->frustumParamsBuf);
                params->invProjMatrix = to_glm(vsg::inverse(grid.projection));
                params->projIsOrtho = std::abs(grid.projection[3][3] - 1.0) < 1e-9 ? 1 : 0;
                params->viewport = glm::ivec4(grid.viewport[0], grid.viewport[1], grid.viewport[2], grid.viewport[3]);
                params->numTiles = glm::uvec2(
                    (grid.viewport[2] + FRUSTUM_GRID_TILE_SIZE_PIXELS - 1u) / FRUSTUM_GRID_TILE_SIZE_PIXELS,
                    (grid.viewport[3] + FRUSTUM_GRID_TILE_SIZE_PIXELS - 1u) / FRUSTUM_GRID_TILE_SIZE_PIXELS);
                params->pixelsPerTile = FRUSTUM_GRID_TILE_SIZE_PIXELS; // assuming square tiles
                params.dirty();

                // Allocate space on the GPU for all the actual frustums and update the buffer.
                // This will require a recompile of course.

                Log()->debug("FrustumGridSystem: updating frustum grid for view {} to {}x{} at {} bytes",
                    grid.viewID, params->numTiles.x, params->numTiles.y,
                    params->numTiles.x * params->numTiles.y * sizeof(FrustumGPU));


                GPUOnlyBufferAccess<FrustumGPU> frustums(vds->frustumsBuf);
                frustums.resize(params->numTiles.x * params->numTiles.y, vsgcontext->device(), vsgcontext);

                // resizing the buffer qequires rebuilding the descriptor sets that hold it
                _sharedRenderData->rebuildVdsDescriptorSet(grid.viewID, vsgcontext);

                // rebuild the commands to incorporate any new buffers we created
                dispose(view.commands);

                auto pipelineLayout = vsg::PipelineLayout::create(
                    vsg::DescriptorSetLayouts{
                        vsg::DescriptorSetLayout::create(), // note: layout 0 is empty
                        vds->descriptorSet->setLayout },
                        vsg::PushConstantRanges{});

                auto pipeline = vsg::ComputePipeline::create(pipelineLayout, _shader);

                auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

                // binds the DS to the compute shader:
                auto bindDescriptorSets = vsg::BindDescriptorSet::create(
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->layout,
                    DESCRIPTOR_SET_VDS,
                    vds->descriptorSet);

                // launches the compute shader:
                auto dispatch = vsg::Dispatch::create(
                    (params->numTiles.x + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
                    (params->numTiles.y + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
                    1u);

                // a general-purpose barrier to ensure that the compute shader writes are visible to subsequent reads
                auto pipelineBarrier = vsg::PipelineBarrier::create(
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0, // dependency flags
                    vsg::MemoryBarrier::create(
                        VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT));

                view.commands = vsg::Commands::create();
                view.commands->addChild(bindPipeline);
                view.commands->addChild(bindDescriptorSets);
                view.commands->addChild(dispatch);
                view.commands->addChild(pipelineBarrier);

                requestCompile(view.commands);

                view.newGrid.reset();
            }

            else
            {
                // transmit the current viewport and inverse projection matrix to the GPU for this view
                BufferAccess<FrustumGridParamsGPU> params(vds->frustumParamsBuf);
                auto& vp = vds->viewportData->at(0);
                params->viewport = glm::ivec4(vp[0], vp[1], vp[2], vp[3]);
                params->invProjMatrix = to_glm(vds->view->camera->projectionMatrix->inverse());
                params.dirty();
            }
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

    if (isCompute)
    {
        for (unsigned viewID=0; viewID <_views.size(); ++viewID)
        {
            auto& view = _views[viewID];
            if (view.commands)
            {
                auto& vds = _sharedRenderData->viewDependentState[viewID];

                if (vds)
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
        auto& view = _views[viewID];
        auto& vds = _sharedRenderData->viewDependentState[viewID];

        if (!vds) {
            // view has probably been destroyed but traversals are still in progress..why?
            return;
        }

        // Extract the viewport size:
        auto* viewportData = vds->viewportData.get();
        if (!viewportData || viewportData->empty())
            return;

        const auto& vp = (*viewportData)[0];

        auto& projMatrix = state->projectionMatrixStack.top();

        BufferAccess<FrustumGridParamsGPU> params(vds->frustumParamsBuf);

        if (params->viewport[2] != vp[2] || params->viewport[3] != vp[3] || projMatrix[3][3] != view.lastProjMatrix[3][3])
        {
            // If the viewport size changes we have to reallocate the grid. Queue an update for the next frame.
            Grid newGrid;
            newGrid.viewID = viewID;
            newGrid.viewport = vp;
            newGrid.projection = projMatrix;
            view.newGrid = std::move(newGrid);
            view.lastProjMatrix = projMatrix;
        }
    }
}
