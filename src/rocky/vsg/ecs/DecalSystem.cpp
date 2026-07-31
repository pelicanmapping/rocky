/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "DecalSystem.h"
#include "OpticsSystem.h"
#include "ECSVisitors.h"
#include "../ViewDependentState.h"
#include "../SharedRenderData.h"
#include "../ShaderDefines.h"
#include <rocky/ecs/Optics.h>
#include <rocky/vsg/VSGUtils.h>

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

    struct DecalDetail
    {
        // where is this decal in the SSBO?
        std::int32_t ssboIndex = -1;
    };

    struct DecalStyleDetail
    {
        vsg::ref_ptr<vsg::ImageInfo> texture;

        // where is this texture in the descriptorimage?
        std::int32_t descriptorImageIndex = -1;
    };
}


void DecalSystemNode::on_construct_Decal(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    r.emplace<DecalDetail>(e);
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
    r.remove<DecalDetail>(e);

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
    // nop
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
DecalSystemNode::updateStyles(VSGContext vsgcontext)
{
    // process any objects marked dirty
    auto&& [_, reg] = _registry.read();

    bool sharedDescriptorsDirty = false;

    DecalStyle::eachDirty(reg, [&](entt::entity e)
    {
        const auto& [style, styleDetail] = reg.get<DecalStyle, DecalStyleDetail>(e);
        auto textures = vsgcontext->sharedRenderData->decalTextures;
        if (!textures || textures->imageInfoList.empty())
            return;

        auto fallback = textures->imageInfoList[0]; // slot 0 reserved fallback

        auto releaseSlot = [&]()
        {
            if (styleDetail.descriptorImageIndex > 0 &&
                styleDetail.descriptorImageIndex < (std::int32_t)textures->imageInfoList.size())
            {
                dispose(textures->imageInfoList[styleDetail.descriptorImageIndex]);
                textures->imageInfoList[styleDetail.descriptorImageIndex] = fallback;
                requestCompile(textures);
                sharedDescriptorsDirty = true;
            }
            styleDetail.descriptorImageIndex = -1;
            styleDetail.texture = {};
        };

        if (style.image)
        {
            int slot = styleDetail.descriptorImageIndex;

            // replacing existing style texture
            if (slot > 0 && slot < static_cast<int>(textures->imageInfoList.size()))
            {
                dispose(textures->imageInfoList[slot]);
                textures->imageInfoList[slot] = fallback;
                styleDetail.texture = {};
                sharedDescriptorsDirty = true;
            }

            // make a new texture from the image:
            auto image = wrapImageInVSG(style.image);
            if (image)
            {
                if (slot < 0)
                {
                    // find a free slot; skip slot 0 (fallback)
                    for (std::size_t i = 1; i < textures->imageInfoList.size(); ++i)
                    {
                        if (textures->imageInfoList[i] == fallback)
                        {
                            slot = static_cast<int>(i);
                            break;
                        }
                    }
                }

                if (slot < 0)
                {
                    Log()->warn("DecalSystemNode: out of decal texture slots (MAX_NUM_DECAL_TEXTURES={})", MAX_NUM_DECAL_TEXTURES);
                    releaseSlot();
                    return;
                }

                auto sampler = vsg::Sampler::create();
                // TODO: set up sampler..
                image->properties.dataVariance = vsg::DYNAMIC_DATA;
                styleDetail.texture = vsg::ImageInfo::create(sampler, image);
                textures->imageInfoList[slot] = styleDetail.texture;
                styleDetail.descriptorImageIndex = slot;

                requestCompile(styleDetail.texture);
                requestCompile(textures);
                sharedDescriptorsDirty = true;
            }
            else
            {
                Log()->warn("DecalSystemNode: failed to create texture for decal style");
                releaseSlot();
            }
        }
        else
        {
            // style lost image; release any previous slot
            releaseSlot();
        }
    });

    if (sharedDescriptorsDirty)
    {
        vsgcontext->sharedRenderData->dirtySharedDescriptors();
    }

    Decal::eachDirty(reg, [&](entt::entity e)
    {
        // nop
    });
}

void
DecalSystemNode::resizeGPUBuffersIfNeeded(VSGContext vsgcontext)
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
    BufferAccess<DecalGPU> decals(_sharedRenderData->decalsBuf, BINDING_DECALS, TYPE_DECALS);

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

        // creates a new descriptor buffer, so we have to notify users to rebuild DSets that use it.
        decals.resize(newDecalCapacity + 1, vsgcontext);
        buffersChanged = true;

        // if we changed the buffer we have to also recompile the parenting DS:
        // TODO: do this in response to onDecalsBufReallocated event just for consistency
        if (_localDescriptorSet)
        {
            auto old_ds = _localDescriptorSet;
            _localDescriptorSet = vsg::DescriptorSet::create(old_ds->setLayout, old_ds->descriptors);
            dispose(old_ds);
        }
    }

    for (auto& vds : _sharedRenderData->viewDependentState)
    {
        if (!vds || !vds->frustumParamsBuf)
            break;

        auto& view = _views[vds->view->viewID];

        // See if the frustum grid has changed size, and if so, resize the decal tiles buffer to match.
        BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
        auto numFrustumTiles = params->numTiles.x * params->numTiles.y;

        GPUOnlyBufferAccess<DecalTileGPU> decalTiles(vds->decalTilesBuf);

        auto numDecalTiles = decalTiles.capacity();
        if (numDecalTiles != numFrustumTiles)
        {
            Log()->info("DecalSystemNode: resizing decal tiles buffer from {} to {} tiles at {} bytes", numDecalTiles, numFrustumTiles, numFrustumTiles * sizeof(DecalTileGPU));
            decalTiles.resize(numFrustumTiles, vsgcontext->device(), vsgcontext);
            buffersChanged = true;
        }

        if (buffersChanged)
        {
            _sharedRenderData->rebuildVdsDescriptorSet(vds->view->viewID, vsgcontext);

            dispose(view.commands);
            view.commands = {};
        }

        // make sure we have the commands built.
        if (!view.commands)
        {
            rebuildCommands(vds->view->viewID, vsgcontext);
        }
    }

    if (buffersChanged)
    {
        vsgcontext->sharedRenderData->dirtySharedDescriptors();
    }
}

void
DecalSystemNode::rebuildCommands(ViewIDType viewID, VSGContext vsgcontext)
{
    auto& view = _views[viewID];
    auto& vds = _sharedRenderData->viewDependentState[viewID];

    vsgcontext->dispose(view.commands);

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
            vsg::DescriptorSetLayout::create(), // set 0 (empty)
            vds->descriptorSet->setLayout,  // set 1 (VDS)
            _localDescriptorSet->setLayout, // set 2 (global)
        },
        vsg::PushConstantRanges{}
    );

    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, _cullingShader);

    auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

    auto bindGlobal = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        DESCRIPTOR_SET_GLOBAL,
        _localDescriptorSet);

    auto bindVDS = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        DESCRIPTOR_SET_VDS,
        vds->descriptorSet);

    // launches the compute shader:
    BufferAccess<FrustumGridParams> params(vds->frustumParamsBuf);
    auto dispatch = vsg::Dispatch::create(
        (params->numTiles.x + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
        (params->numTiles.y + FRUSTUM_GRID_TILES_PER_THREAD_GROUP - 1u) / FRUSTUM_GRID_TILES_PER_THREAD_GROUP,
        1u);

    // a general-purpose memory barrier to ensure that the compute shader writes are visible to subsequent reads
    auto memoryBarrier = vsg::MemoryBarrier::create(
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT);

    auto pipelineBarrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, // dependency flags
        memoryBarrier);

    view.commands = vsg::Commands::create();
    view.commands->addChild(bindPipeline);
    view.commands->addChild(bindGlobal);
    view.commands->addChild(bindVDS);
    view.commands->addChild(dispatch);
    view.commands->addChild(pipelineBarrier);

    vsgcontext->compile(view.commands);
}

void
DecalSystemNode::updateDecalsSSBO(VSGContext vsgcontext)
{
    // update for each view:
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

            std::uint32_t decalIndex = 0u;

            auto& vp = vds->viewportData->at(0);
            RenderingState rs{
                viewID, (FrameCountType)~0,
                { vp[0], vp[1], vp[2], vp[3] }
            };

            _registry.read([&](entt::registry& reg)
            {
                auto updateDecal = [&](auto entity, auto& decal, auto& decalDetail, auto& active, auto& visibility, auto& transformDetail)
                {
                    ROCKY_HARD_ASSERT(decalIndex + 1u < gpuCapacity, "DecalSystemNode: decal SSBO overflow");

                    if (!visible(visibility, rs))
                    {
                        return;
                    }

                    auto e_optics = decal.optics != entt::null ? decal.optics : decal.owner;

                    auto [optics, opticsDetails] = reg.try_get<Optics, OpticsDetail>(e_optics);
                    if (!optics)
                        return;

                    auto* opticsDetail = &opticsDetails->views[viewID];

                    // first decal just holds the count, so advance first:
                    ++gpudecal;
                    ++decalIndex;

                    // update the detail record with the index of this decal in the SSBO,
                    // so we can find it later if we need to change the style
                    decalDetail.ssboIndex = decalIndex;
                    
                    
                    // store the texture arena index if we have a texture; else -1 to indicate no texture.
                    gpudecal->opacity = 1.0f;
                    gpudecal->textureIndex = -1;

                    auto e_style = decal.style != entt::null ? decal.style : decal.owner;
                    auto [style, styleDetail] = reg.try_get<DecalStyle, DecalStyleDetail>(e_style);
                    if (style)
                    {
                        gpudecal->opacity = style->opacity;
                        gpudecal->textureIndex = styleDetail->descriptorImageIndex;
                    }

                    auto mvm = vm * to_glm(transformDetail.views[viewID].model);

                    if (optics->projection == Optics::Projection::Perspective)
                    {
                        auto* opticsTransformDetail = reg.try_get<TransformDetail>(e_optics);
                        if (!opticsTransformDetail)
                            return;

                        // Build projector world matrix from optics owner transform + optics local pose.
                        glm::dmat4 opticsModel = to_glm(opticsTransformDetail->views[viewID].model) * optics->pose;

                        // Strip scale from projector basis so metric near/far/focal values remain valid.
                        glm::dvec3 x(opticsModel[0]);
                        glm::dvec3 y(opticsModel[1]);
                        glm::dvec3 z(opticsModel[2]);

                        double lx = glm::length(x);
                        double ly = glm::length(y);
                        double lz = glm::length(z);
                        if (lx <= 0.0 || ly <= 0.0 || lz <= 0.0)
                            return;

                        x /= lx;
                        y /= ly;
                        z /= lz;

                        opticsModel[0] = glm::dvec4(x, 0.0);
                        opticsModel[1] = glm::dvec4(y, 0.0);
                        opticsModel[2] = glm::dvec4(z, 0.0);

                        auto opticsMvm = vm * opticsModel;
                        gpudecal->mvm = glm::fmat4(opticsMvm);
                        // NB: gpudecal->mvmInverse is computed in the culling shader.

                        float tanH = tanf(glm::radians((float)optics->fovY * 0.5f));
                        float nearClip = std::max(1.0f, (float)opticsDetail->nearDistance);
                        float farClip = std::max(nearClip + 1.0f, (float)opticsDetail->farDistance);

                        float halfDepth = 0.5f * (farClip - nearClip);
                        float farHalfW = farClip * tanH * (float)optics->aspectRatio;
                        float farHalfH = farClip * tanH;
                        float bsRadius = sqrtf(farHalfW * farHalfW + farHalfH * farHalfH + halfDepth * halfDepth);

                        // Projector looks down local -Z; visible range is [-far, -near].
                        gpudecal->zMin = -farClip;
                        gpudecal->zMax = -nearClip;
                        gpudecal->cullingRadius = bsRadius;
                        gpudecal->distance = 1.0f; // perspective flag (>0)
                        gpudecal->tanHalfFovY = tanH;
                        gpudecal->aspect = (float)optics->aspectRatio;
                    }
                    else
                    {
                        // Orthographic remains decal-transform-based (includes scale as box extents).
                        gpudecal->mvm = glm::fmat4(mvm * optics->pose);
                        // NB: gpudecal->mvmInverse is computed in the culling shader.
                        gpudecal->distance = 0.0f; // zero means orthographic
                    }
                };

                reg.view<Decal, DecalDetail, ActiveState, Visibility, TransformDetail>().each(updateDecal);
            });

            // Keep the shader-side loop bound aligned with what we actually wrote.
            BufferAccess<DecalGPU> decalHeader(_sharedRenderData->decalsBuf);
            decalHeader->count = decalIndex;
            decalHeader.dirty();
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

    updateStyles(vsgcontext);

    resizeGPUBuffersIfNeeded(vsgcontext);

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