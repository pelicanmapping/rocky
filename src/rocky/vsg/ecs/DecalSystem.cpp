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
        glm::mat4 projection;
        glm::mat4 modelview;
    };

    struct GPUDecalTile
    {
        std::uint32_t count;
        std::uint32_t indices[MAX_DECALS_PER_TILE];
        std::uint32_t padding[3];
    };

    struct GPUDecal
    {
        glm::fmat4 mvm;
        glm::fmat4 mvmInverse;
        union { 
            glm::float32 halfX;
            glm::float32 zMin;
        };
        union {
            glm::float32 halfY;
            glm::float32 zMax;
        };
        union {
            glm::float32 halfZ;
            glm::float32 cullingRadius;
        };
        union {
            std::int32_t textureIndex = -1; // when index > 0
            std::int32_t count; // when index == 0
        };
        float opacity = 1.0f;
        float distance = 0.0f; // > 0 = persp
        float tanHalfFovY = 0.0f;
        float aspect = 1.0f;
    };

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
            r.on_destroy<DecalStyle>().connect<&DecalSystemNode::on_destroy_DecalStyle>(*this);
            r.on_destroy<DecalStyleDetail>().connect<&DecalSystemNode::on_destroy_DecalStyleDetail>(*this);

            // Set up the dirty tracking
            auto e = r.create();
            r.emplace<Decal::Dirty>(e);
            r.emplace<DecalStyle::Dirty>(e);
        });
}

void
DecalSystemNode::growGPUBuffersIfNeeded()
{
    bool buffersChanged = false;

    // Decal input buffer: keep room for all decals (+1 entry at index 0 for count).
    std::uint32_t currentDecalCapacity = _decalsData ?
        static_cast<std::uint32_t>(_decalsData->size() / sizeof(GPUDecal)) : 0u;
    if (currentDecalCapacity > 0u)
        --currentDecalCapacity;

    // If the buffer doesn't yet exist, OR it needs to grow,
    // make that happen here:
    if (!_decalsData || _totalNumDecals > currentDecalCapacity)
    {
        const std::uint32_t growBy = 32u;

        const std::uint32_t newDecalCapacity = (_totalNumDecals > currentDecalCapacity + growBy) ?
            _totalNumDecals :
            (currentDecalCapacity + growBy);

        _decalsData = vsg::ubyteArray::create((newDecalCapacity + 1u) * sizeof(GPUDecal));

        _decalsBuf = vsg::DescriptorBuffer::create(
            _decalsData, BINDING_DECALS, 0, TYPE_DECALS);

        requestCompile(_decalsBuf);
        buffersChanged = true;
    }

    // Decal tile output buffer must match frustum tile capacity from FrustumGridSystem.
    // Find the largest one.
    std::uint32_t requiredTileCapacity = 0u;
    if (_sharedRenderData)
    {
        for (auto& vds : _sharedRenderData->viewDependentState)
        {
            if (!vds->frustumsBuf)
                continue;

            BufferAccess<FrustumGridParams> params(vds->frustumsBuf);

            auto numTiles = params->numTiles.x * params->numTiles.y;

            if (numTiles > requiredTileCapacity)
                requiredTileCapacity = numTiles;
        }
    }

    if (requiredTileCapacity > 0u)
    {
        const std::uint32_t currentTileCapacity = _decalTilesData ?
            static_cast<std::uint32_t>(_decalTilesData->size() / sizeof(GPUDecalTile)) : 0u;

        if (!_decalTilesData || requiredTileCapacity > currentTileCapacity)
        {
            _decalTilesData = vsg::ubyteArray::create(requiredTileCapacity * sizeof(GPUDecalTile));

            _decalTilesBuf = vsg::DescriptorBuffer::create(
                _decalTilesData, BINDING_DECAL_TILES, 0, TYPE_DECAL_TILES);

            requestCompile(_decalTilesBuf);
            buffersChanged = true;
        }
    }

    if (buffersChanged)
    {
        for (auto& view : _views)
        {
            dispose(view.bindDescriptorSet);
            dispose(view.commands);
            view.bindDescriptorSet = nullptr;
            view.commands = nullptr;
            view.lastFrustumParamsBuf = nullptr;
            view.lastFrustumsBuf = nullptr;
        }
    }
}

void
DecalSystemNode::initialize(VSGContext vsgcontext)
{
#if 0 // COMMENTED OUT WHILE BUILDING OTHER MODULES
    auto shader = vsg::ShaderStage::read(
        VK_SHADER_STAGE_COMPUTE_BIT,
        "main",
        vsg::findFile(DECAL_CULLING_SHADER, vsgcontext->searchPaths),
        vsgcontext->readerWriterOptions);

    if (!shader)
    {
        status = Failure(Failure::ResourceUnavailable, "DecalSystemNode: missing compute shader");
        return;
    }

    // custom PC data to send the modelview matrix to the compute shader:
    // TODO: delete these, don't need 'em
    //_pcData = vsg::ubyteArray::create(sizeof(MyPushConstants));
    //_pcCommand = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, _pcData);

    // the buffer objects we will bind to the compute shader
    vsg::DescriptorSetLayoutBindings bindings = {
        { BINDING_FRUSTUM_GRID_PARAMS, TYPE_FRUSTUM_GRID_PARAMS, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { BINDING_FRUSTUMS,            TYPE_FRUSTUMS,            1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { BINDING_DECALS,              TYPE_DECALS,              1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { BINDING_DECAL_TILES,         TYPE_DECAL_TILES,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr }
    };
    _descriptorSetLayout = vsg::DescriptorSetLayout::create(bindings);

    auto pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{ _descriptorSetLayout },
        vsg::PushConstantRanges {});

    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, shader);

    _bindPipeline = vsg::BindComputePipeline::create(pipeline);

    // we'll update our group sizes before each dispatch:
    _dispatch = vsg::Dispatch::create(1, 1, 1);

    // we will do this later in traverse() when we have the frustum grid SSBOs available
    //requestCompile(_bindPipeline);

    _sharedRenderData = vsgcontext->sharedRenderData;
#endif
}

void
DecalSystemNode::update(VSGContext vsgcontext)
{
    if (status.failed())
        return;

    growGPUBuffersIfNeeded();

    Inherit::update(vsgcontext);
}


void
DecalSystemNode::traverse(vsg::RecordTraversal& record) const
{
#if 0 // COMMENTED OUT WHILE BUILDING OTHER MODULES
    if (status.failed())
        return;

    if (!_sharedRenderData)
        return;

    auto vp = record.getCommandBuffer()->viewDependentState->view->camera->getViewport();
    RenderingState rs{
        record.getCommandBuffer()->viewID,
        record.getFrameStamp()->frameCount,
        { vp.x, vp.y, vp.x + vp.width, vp.y + vp.height }
    };

    auto& fg = _sharedRenderData->frustumGrid[rs.viewID];
    auto frustumParamsBuf = fg.paramsBuf;
    auto frustumsBuf = fg.frustumsBuf;

    auto& view = _views[rs.viewID];

    if (!frustumParamsBuf || !frustumsBuf || !_decalsBuf || !_decalTilesBuf)
        return;

    // if the buffers change, we will need to rebuild our command list.
    if (view.commands &&
        (view.lastFrustumParamsBuf != frustumParamsBuf ||
         view.lastFrustumsBuf != frustumsBuf))
    {
        dispose(view.bindDescriptorSet);
        dispose(view.commands);
        view.bindDescriptorSet = nullptr;
        view.commands = nullptr;
    }

    // Assemble the decriptor set; we have to do this here because the frustum grid SSBOs
    // convey through the record traversal aux data.
    // TODO: possibly reevaluate this approach.
    if (!view.commands)
    {
        // the descriptor set that will bind our 4 SSBOs to the culling shader.
        // Make sure the descriptors appear in the same order as in the DS layout.
        auto descriptorSet = vsg::DescriptorSet::create(
            _descriptorSetLayout,
            vsg::Descriptors{
                frustumParamsBuf, frustumsBuf, _decalsBuf, _decalTilesBuf });

        view.bindDescriptorSet = vsg::BindDescriptorSet::create(
            VK_PIPELINE_BIND_POINT_COMPUTE, _bindPipeline->pipeline->layout, 0, descriptorSet);

        // TODO: create a barrier??

        view.commands = vsg::Commands::create();
        view.commands->addChild(_bindPipeline);
        view.commands->addChild(view.bindDescriptorSet);
        view.commands->addChild(_dispatch);

        requestCompile(view.commands);

        view.lastFrustumParamsBuf = frustumParamsBuf;
        view.lastFrustumsBuf = frustumsBuf;

        // actual compile will happen on next update, so we are done for now
        return;
    }

    // First: update the push constants for this view.
    //auto* pc = reinterpret_cast<MyPushConstants*>(_pcData->dataPointer());
    //pc->projection = to_glm(record.state->projectionMatrixStack.top());
    //pc->modelview = to_glm(record.state->modelviewMatrixStack.top());

    // TODO: iterate the decals and build a draw list
    BufferAccess<GPUDecal> gpudecal(_decalsBuf);

    // the first entry just holds the count.
    gpudecal->count = _totalNumDecals;
    ++gpudecal;

    auto& mvm = record.state->modelviewMatrixStack.top();

    _registry.read([&](entt::registry& reg)
        {
            auto iter = reg.view<Decal, ActiveState, Visibility>();
            iter.each([&](auto entity, auto& decal, auto& active, auto& visibility)
            {
                gpudecal->mvm = decal.matrix * to_glm(mvm);
                gpudecal->mvmInverse = glm::inverse(gpudecal->mvm);

                if (decal.projection == DecalProjection::Perspective)
                {
                    float tanH = tanf(glm::radians(decal.fovY_deg * 0.5f));
                    float halfDepth = decal.size.z * 0.5f;
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
                    gpudecal->distance = 0.0f;
                    gpudecal->tanHalfFovY = 0.0f;
                    gpudecal->aspect = 1.0f;
                }

                ++gpudecal; // advance to next decal instance in the SSBO
            });
        });

    // TODO: UPLOAD IMMEDIATELY. NEED TO WRITE CODE FOR THIS.
    // TODO: See about repurposing TransferTask.
    // TODO: for now (testing) we will be a frame behind.

    // dispatch the cull shader
    BufferAccess<FrustumGridParams> params(fg.paramsBuf);
    _dispatch->groupCountX = (params->numTiles.x + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP;
    _dispatch->groupCountY = (params->numTiles.y + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP;

    view.commands->accept(record);

    // TODO: barrier???
#endif
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