/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "DecalSystem.h"
#include "ECSVisitors.h"
#include "../ViewDependentState.h"
#include "../SharedRenderData.h"
#include "../ShaderDefines.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define DECAL_CULLING_SHADER "shaders/rocky.decal.cull.comp"

namespace
{
    struct MyPushConstants
    {
        glm::mat4 modelview;
        glm::mat4 projection;
    };

#if 0
    struct DecalTileGPU
    {
        std::uint32_t count = 0;
        std::uint32_t indices[MAX_DECALS_PER_TILE];
        std::uint32_t padding[3];
    };
#endif

    struct DecalStyleDetail
    {
        bool dummy;
    };
}


void DecalSystemNode::on_construct_Decal(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    Decal::dirty(r, e);
    ++_totalNumDecals;
}
void DecalSystemNode::on_construct_DecalStyle(entt::registry& r, entt::entity e)
{
    r.emplace<DecalStyleDetail>(e);
    DecalStyle::dirty(r, e);
}

void DecalSystemNode::on_destroy_Decal(entt::registry& r, entt::entity e)
{
    ROCKY_SOFT_ASSERT(_totalNumDecals > 0, "DecalSystemNode: decal count mismatch");
    if (_totalNumDecals > 0)
        --_totalNumDecals;
}

void DecalSystemNode::on_destroy_DecalStyle(entt::registry& r, entt::entity e)
{
    r.remove<DecalStyleDetail>(e);
}
void DecalSystemNode::on_destroy_DecalStyleDetail(entt::registry& r, entt::entity e)
{
    auto& d = r.get<DecalStyleDetail>(e);
    //dispose(d.bind);
}

void DecalSystemNode::on_update_Decal(entt::registry& r, entt::entity e)
{
    Decal::dirty(r, e);
}
void DecalSystemNode::on_update_DecalStyle(entt::registry& r, entt::entity e)
{
    DecalStyle::dirty(r, e);
}





DecalSystemNode::DecalSystemNode(Registry& registry) :
    Inherit(registry)
{
    _registry.write([&](entt::registry& r)
        {
            // install the ecs callbacks for Decals
            r.on_construct<Decal>().connect<&DecalSystemNode::on_construct_Decal>(*this);
            r.on_construct<DecalStyle>().connect<&DecalSystemNode::on_construct_DecalStyle>(*this);
            r.on_update<Decal>().connect<&DecalSystemNode::on_update_Decal>(*this);
            r.on_update<DecalStyle>().connect<&DecalSystemNode::on_update_DecalStyle>(*this);
            r.on_destroy<Decal>().connect<&DecalSystemNode::on_destroy_Decal>(*this);
            r.on_destroy<DecalStyle>().connect<&DecalSystemNode::on_destroy_DecalStyle>(*this);
            r.on_destroy<DecalStyleDetail>().connect<&DecalSystemNode::on_destroy_DecalStyleDetail>(*this);

            // Set up the dirty tracking
            auto e = r.create();
            r.emplace<Decal::Dirty>(e);
            r.emplace<DecalStyle::Dirty>(e);

            // Seed the running decal count in case decals already exist.
            _totalNumDecals = 0u;
            auto existing = r.view<Decal>();
            existing.each([&](auto) { ++_totalNumDecals; });
        });
}

void
DecalSystemNode::growGPUBuffersIfNeeded(VSGContext vsgcontext)
{
    bool buffersChanged = false;

    // Recount from registry to keep capacity decisions in sync with actual entities.
    _registry.read([&](entt::registry& reg)
    {
        _totalNumDecals = 0u;
        auto decals = reg.view<Decal, ActiveState, Visibility, TransformDetail>();
        decals.each([&](auto, auto&, auto&, auto&, auto&) { ++_totalNumDecals; });
    });

    if (!_sharedRenderData->decalsBuf)
        buffersChanged = true;

    // Decal input buffer: keep room for all decals (+1 entry at index 0 for count).
    BufferAccess<DecalGPU> decals(_sharedRenderData->decalsBuf, BINDING_DECALS, TYPE_DECALS, 1u);
    auto currentDecalsCapacity = decals.capacity();
    if (currentDecalsCapacity > 0u)
        --currentDecalsCapacity;

    decals->count = _totalNumDecals;

    // Does the decals buffer need to grow?
    if (_totalNumDecals > currentDecalsCapacity)
    {
        constexpr std::size_t growBy = 32u;

        const auto newDecalCapacity = (_totalNumDecals > currentDecalsCapacity + growBy) ?
            _totalNumDecals :
            (currentDecalsCapacity + growBy);

        auto old = decals.resize(newDecalCapacity + 1); // +1 for count at index 0
        dispose(old);
        requestCompile(_sharedRenderData->decalsBuf);

        // if we changed the buffer we have to also recompile the parenting DS:
        // TODO: do this in response to onDecalsBufReallocated event just for consistency
        if (_localDescriptorSet)
        {
            _localDescriptorSet->release();
            vsg::Context context(vsgcontext->device());
            _localDescriptorSet->compile(context);
        }

        _sharedRenderData->onDecalsBufReallocated.fire();
        
        buffersChanged = true;
    }

    if (_sharedRenderData)
    {
        for (auto& vds : _sharedRenderData->viewDependentState)
        {
            if (!vds || !vds->frustumParamsBuf)
                break;

            auto& view = _views[vds->view->viewID];

            // See if the frustum grid has changed size, and if so, resize the decal tiles buffer to match.
            BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
            auto numFrustumTiles = params->numTiles.x * params->numTiles.y;

            GPUOnlyBufferAccess<DecalTileGPU> tiles(vds->decalTilesBuf, vsgcontext->device());

            auto capacity = tiles.capacity();
            if (capacity < numFrustumTiles)
            {
                Log()->info("DecalSystemNode: growing decal tiles buffer from {} to {} tiles at {} bytes", capacity, numFrustumTiles, numFrustumTiles * sizeof(DecalTileGPU));
                auto old = tiles.resize(numFrustumTiles);
                dispose(old);
                requestCompile(vds->decalTilesBuf);

                buffersChanged = true;
            }

            if (buffersChanged)
            {
                // reallocating buffers reuqires rebuilding the descriptor sets that hold them.
                vds->recompileDescriptorSets();
                dispose(view.commands);
                view.commands = {};
            }

            // make sure we have the commands built.
            if (!view.commands)
            {
                view.commands = vsg::Commands::create();

                // use the ds layout from the first view-dependent state, which is shared by all views:
                auto vdsDescriptorSetLayout = _sharedRenderData->viewDependentState[0]->descriptorSetLayout;

                // a local DSL to capture our decals buffer (which is not view-dependent, but is shared by all views):

                _localDescriptorSet = vsg::DescriptorSet::create();
                _localDescriptorSet->setLayout = vsg::DescriptorSetLayout::create();
                _localDescriptorSet->setLayout->addBinding(BINDING_DECALS, TYPE_DECALS, 1, VK_SHADER_STAGE_ALL);
                _localDescriptorSet->descriptors.emplace_back(_sharedRenderData->decalsBuf);

                auto pipelineLayout = vsg::PipelineLayout::create(
                    vsg::DescriptorSetLayouts{
                        _localDescriptorSet->setLayout, // set 0 (local)
                        vds->descriptorSet->setLayout   // set 1 (VDS)
                    }, 
                    vsg::PushConstantRanges {}
                );

                auto pipeline = vsg::ComputePipeline::create(pipelineLayout, _cullingShader);

                auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

                // binds the DS to the compute shader:
                auto bindDescriptorSets = vsg::BindDescriptorSets::create(
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->layout,
                    vsg::DescriptorSets {
                        _localDescriptorSet,
                        vds->descriptorSet
                    });

                // sends the matrices to the culling shader:
                //auto pushConstants = vsg::PushConstants::create(
                //    pipelineLayout,
                //    VK_SHADER_STAGE_COMPUTE_BIT,
                //    0, sizeof(MyPushConstants),
                //    _pushConstantsData);

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
                    vds->decalTilesBuf->bufferInfoList[0]->buffer,
                    0, VK_WHOLE_SIZE);

                auto barrier = vsg::PipelineBarrier::create(
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0, // dependency flags
                    bufferBarrier);

                view.commands = vsg::Commands::create();
                view.commands->addChild(bindPipeline);
                view.commands->addChild(bindDescriptorSets);
                view.commands->addChild(dispatch);
                view.commands->addChild(barrier);

                vsgcontext->compile(view.commands);
            }
        }
    }
}

void
DecalSystemNode::updateDecalsSSBO(VSGContext vsgcontext)
{
    // run the culling shader on ALL views
    for (unsigned viewID = 0; viewID < _views.size(); ++viewID)
    {
        auto& view = _views[viewID];
        auto& vds = _sharedRenderData->viewDependentState[viewID];

        if (view.commands && vds)
        {
            auto vm = to_glm(vds->view->camera->viewMatrix->transform());

            BufferAccess<DecalGPU> gpudecal(_sharedRenderData->decalsBuf);
            const auto gpuCapacity = gpudecal.capacity();
            if (gpuCapacity == 0u)
                continue;

            std::uint32_t written = 0u;

            // Resolve the local matrix for each bbox
            _registry.read([&](entt::registry& reg)
            {
                auto updateDecal = [&](auto entity, auto& decal, auto& active, auto& visibility, auto& transformDetail)
                {
                    // slot 0 is the count header
                    if (written + 1u >= gpuCapacity)
                        return;

                    // first decal just holds the count, so advance first:
                    ++gpudecal;
                    ++written;

                    // Ensure a fully initialized record every frame.
                    gpudecal->count = 0;

                    //auto mvm = ;
                    auto model = to_glm(transformDetail.views[viewID].model);
                    auto mvm = vm * model;

                    auto localScale = glm::dvec3(
                        glm::length(glm::dvec3(mvm[0])), //transformDetail.sync.localMatrix[0])),
                        glm::length(glm::dvec3(mvm[1])), //transformDetail.sync.localMatrix[1])),
                        glm::length(glm::dvec3(mvm[2]))); //transformDetail.sync.localMatrix[2])));

                    if (localScale.x <= 0.0) localScale.x = 1.0;
                    if (localScale.y <= 0.0) localScale.y = 1.0;
                    if (localScale.z <= 0.0) localScale.z = 1.0;

                    gpudecal->mvm = glm::fmat4(mvm);
                    // NB: gpudecal->mvmInverse is computed in the culling shader.

                    if (decal.projection == DecalProjection::Perspective)
                    {
                        float tanH = tanf(glm::radians(decal.fovY_deg * 0.5f));
                        float halfDepth = static_cast<float>(localScale.z * 0.5);
                        float nearClip = glm::max(1.0f, decal.distance - halfDepth);
                        float farClip = decal.distance + halfDepth;
                        float farHalfW = farClip * tanH * decal.aspectRatio;
                        float farHalfH = farClip * tanH;
                        float bsRadius = sqrtf(farHalfW * farHalfW + farHalfH * farHalfH);

                        gpudecal->zMin = decal.distance - farClip;
                        gpudecal->zMax = decal.distance - nearClip;
                        gpudecal->cullingRadius = bsRadius;
                        gpudecal->distance = decal.distance;
                        gpudecal->tanHalfFovY = tanH;
                        gpudecal->aspect = decal.aspectRatio;
                    }
                    else
                    {
                        gpudecal->halfX = static_cast<float>(localScale.x * 0.5);
                        gpudecal->halfY = static_cast<float>(localScale.y * 0.5);
                        gpudecal->halfZ = static_cast<float>(localScale.z * 0.5);
                        gpudecal->distance = 0.0f;
                        gpudecal->tanHalfFovY = 0.0f;
                        gpudecal->aspect = 1.0f;
                    }
                };

                reg.view<Decal, ActiveState, Visibility, TransformDetail>().each(updateDecal);
            });

            // Keep the shader-side loop bound aligned with what we actually wrote.
            BufferAccess<DecalGPU> decalHeader(_sharedRenderData->decalsBuf);
            decalHeader->count = static_cast<std::int32_t>(written);

            requestUpload(_sharedRenderData->decalsBuf->bufferInfoList);
        }
        else
        {
            // detect a removed view and dispose of its contents
            if (view.commands)
                dispose(view.commands);

            view.commands = {};
        }
    }
}

void
DecalSystemNode::initialize(VSGContext vsgcontext)
{
    _sharedRenderData = vsgcontext->sharedRenderData;

    // no likey
    _vsgcontext = vsgcontext;

    _cullingShader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_COMPUTE_BIT,
        "main",
        vsg::findFile(DECAL_CULLING_SHADER, vsgcontext->searchPaths),
        vsgcontext->readerWriterOptions);

    if (!_cullingShader)
    {
        status = Failure(Failure::ResourceUnavailable, "DecalSystemNode: missing compute shader");
        return;
    }

    // convey #define settings to our new shader
    _cullingShader->module->hints = vsgcontext->shaderCompileSettings;
}

void
DecalSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    growGPUBuffersIfNeeded(vsgcontext);

    updateDecalsSSBO(vsgcontext);

    Inherit::update(vsgcontext);
}


void
DecalSystemNode::traverse(vsg::RecordTraversal& record) const
{
    if (status.failed())
        return;

    ROCKY_SOFT_ASSERT_AND_RETURN(_sharedRenderData, void());

    for(auto& view : _views)
    {
        if (view.commands)
            view.commands->accept(record);
        else
            break;
    }    
}

void
DecalSystemNode::traverse(vsg::ConstVisitor& v) const
{
    //TODO - handle intersections (maybe) and other const visitors
    Inherit::traverse(v);
}

void
DecalSystemNode::traverse(vsg::Visitor& v)
{
    //TODO
    Inherit::traverse(v);
}