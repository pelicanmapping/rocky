/**
/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "DecalSystem.h"
#include "OverlayBakeSystem.h"
#include "OpticsSystem.h"
#include "ECSVisitors.h"
#include "../ViewDependentState.h"
#include "../SharedRenderData.h"
#include "../ShaderDefines.h"
#include <rocky/ecs/Optics.h>
#include <rocky/vsg/VSGUtils.h>
#include <algorithm>
#include <cmath>

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

    struct OverlayDetail
    {
        // where is this overlay in the SSBO?
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

void DecalSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
    (void)r.get_or_emplace<DecalStyleDetail>(e);
    r.emplace<OverlayDetail>(e);
    Overlay::dirty(r, e);
    ++_totalNumDecals;
}

void DecalSystemNode::on_construct_DecalStyle(entt::registry& r, entt::entity e)
{
    (void)r.get_or_emplace<DecalStyleDetail>(e);
    DecalStyle::dirty(r, e);
}

void DecalSystemNode::on_destroy_Decal(entt::registry& r, entt::entity e)
{
    r.remove<DecalDetail>(e);

    ROCKY_SOFT_ASSERT(_totalNumDecals > 0, "DecalSystemNode: decal count mismatch");
    if (_totalNumDecals > 0)
        --_totalNumDecals;
}

void DecalSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (!r.any_of<DecalStyle>(e))
        r.remove<DecalStyleDetail>(e);
    r.remove<OverlayDetail>(e);

    ROCKY_SOFT_ASSERT(_totalNumDecals > 0, "DecalSystemNode: decal count mismatch");
    if (_totalNumDecals > 0)
        --_totalNumDecals;
}

void DecalSystemNode::on_destroy_DecalStyle(entt::registry& r, entt::entity e)
{
    if (!r.any_of<Overlay>(e))
        r.remove<DecalStyleDetail>(e);
}

void DecalSystemNode::on_destroy_DecalStyleDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<DecalStyleDetail>(e);
    if (detail.descriptorImageIndex > 0)
    {
        std::scoped_lock lock(_pendingTextureSlotsMutex);
        _pendingTextureSlots.push_back(detail.descriptorImageIndex);
    }
}

void DecalSystemNode::on_update_Decal(entt::registry& r, entt::entity e)
{
    Decal::dirty(r, e);
}

void DecalSystemNode::on_update_Overlay(entt::registry& r, entt::entity e)
{
    Overlay::dirty(r, e);
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
            r.on_construct<Overlay>().connect<&DecalSystemNode::on_construct_Overlay>(*this);
            r.on_update<Decal>().connect<&DecalSystemNode::on_update_Decal>(*this);
            r.on_update<DecalStyle>().connect<&DecalSystemNode::on_update_DecalStyle>(*this);
            r.on_update<Overlay>().connect<&DecalSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Decal>().connect<&DecalSystemNode::on_destroy_Decal>(*this);
            r.on_destroy<DecalStyle>().connect<&DecalSystemNode::on_destroy_DecalStyle>(*this);
            r.on_destroy<Overlay>().connect<&DecalSystemNode::on_destroy_Overlay>(*this);
            r.on_destroy<DecalStyleDetail>().connect<&DecalSystemNode::on_destroy_DecalStyleDetail>(*this);

            // Set up the dirty tracking
            auto e = r.create();
            r.emplace<Decal::Dirty>(e);
            r.emplace<DecalStyle::Dirty>(e);
            r.emplace<Overlay::Dirty>(e);

            // Seed the running decal count in case decals already exist.
            _totalNumDecals = 0u;
            auto existing = r.view<Decal>();
            existing.each([&](auto) { ++_totalNumDecals; });
            auto existingOverlays = r.view<Overlay>();
            existingOverlays.each([&](auto) { ++_totalNumDecals; });
        });
}

void
DecalSystemNode::updateStyles(VSGContext vsgcontext)
{
    // process any objects marked dirty
    auto reader = _registry.read();
    auto& reg = reader.registry;

    bool sharedDescriptorsDirty = false;

    // Component destruction cannot safely edit the shared descriptor arena, so
    // return any slots queued by on_destroy_DecalStyleDetail now.
    {
        std::vector<std::int32_t> pendingSlots;
        {
            std::scoped_lock lock(_pendingTextureSlotsMutex);
            pendingSlots.swap(_pendingTextureSlots);
        }

        auto textures = vsgcontext->sharedRenderData->decalTextures;
        if (textures && !textures->imageInfoList.empty() && !pendingSlots.empty())
        {
            auto fallback = textures->imageInfoList[0];
            std::sort(pendingSlots.begin(), pendingSlots.end());
            pendingSlots.erase(std::unique(pendingSlots.begin(), pendingSlots.end()), pendingSlots.end());

            for (auto slot : pendingSlots)
            {
                if (slot > 0 && slot < static_cast<std::int32_t>(textures->imageInfoList.size()))
                {
                    auto old = textures->imageInfoList[slot];
                    if (old && old != fallback)
                        dispose(old);
                    textures->imageInfoList[slot] = fallback;
                }
            }

            requestCompile(textures);
            sharedDescriptorsDirty = true;
        }
    }

    auto processDecalStyle = [&](const DecalStyle& style, DecalStyleDetail& styleDetail)
    {
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

        auto assignImageInfo = [&](vsg::ref_ptr<vsg::ImageInfo> info)
        {
            int slot = styleDetail.descriptorImageIndex;

            if (slot < 0)
            {
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

            auto old = textures->imageInfoList[slot];
            if (old && old != fallback && old != info)
                dispose(old);
            textures->imageInfoList[slot] = info;
            styleDetail.descriptorImageIndex = slot;
            styleDetail.texture = info;

            requestCompile(info);
            requestCompile(textures);
            sharedDescriptorsDirty = true;
        };

        if (style.image)
        {
            // replacing existing style texture
            if (styleDetail.descriptorImageIndex > 0 && styleDetail.descriptorImageIndex < static_cast<int>(textures->imageInfoList.size()))
            {
                dispose(textures->imageInfoList[styleDetail.descriptorImageIndex]);
                textures->imageInfoList[styleDetail.descriptorImageIndex] = fallback;
            }

            // make a new texture from the image:
            auto image = wrapImageInVSG(style.image);
            if (image)
            {
                auto sampler = vsg::Sampler::create();
                // TODO: set up sampler..
                image->properties.dataVariance = vsg::DYNAMIC_DATA;
                auto info = vsg::ImageInfo::create(sampler, image);
                assignImageInfo(info);
            }
            else
            {
                Log()->warn("DecalSystemNode: failed to create texture for decal style");
                releaseSlot();
            }
        }
        else if (styleDetail.texture)
        {
            assignImageInfo(styleDetail.texture);
        }
        else
        {
            // style lost image; release any previous slot
            releaseSlot();
        }
    };

    auto processOverlayStyle = [&](entt::entity e_overlay, DecalStyleDetail& styleDetail)
    {
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

        auto assignImageInfo = [&](vsg::ref_ptr<vsg::ImageInfo> info)
        {
            int slot = styleDetail.descriptorImageIndex;

            if (slot < 0)
            {
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

            auto old = textures->imageInfoList[slot];
            if (old && old != fallback && old != info)
                dispose(old);
            textures->imageInfoList[slot] = info;
            styleDetail.descriptorImageIndex = slot;
            styleDetail.texture = info;

            requestCompile(info);
            requestCompile(textures);
            sharedDescriptorsDirty = true;
        };

        auto* baked = reg.try_get<OverlayBakeTexture>(e_overlay);

        if (baked && baked->texture)
        {
            assignImageInfo(baked->texture);
        }
        else
        {
            releaseSlot();
        }
    };

    DecalStyle::eachDirty(reg, [&](entt::entity e)
    {
        auto* style = reg.try_get<DecalStyle>(e);
        auto* styleDetail = reg.try_get<DecalStyleDetail>(e);
        if (style && styleDetail)
            processDecalStyle(*style, *styleDetail);
    });

    Overlay::eachDirty(reg, [&](entt::entity e)
    {
        auto* styleDetail = reg.try_get<DecalStyleDetail>(e);
        if (reg.any_of<Overlay>(e) && styleDetail)
            processOverlayStyle(e, *styleDetail);
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
        auto overlays = reg.view<Overlay, ActiveState, Visibility, TransformDetail>();
        overlays.each([&](auto e, auto&, auto&, auto&, auto&) {
            if (!reg.any_of<Decal>(e))
                ++_totalNumDecals;
        });
    });

    for (auto& vds : _sharedRenderData->viewDependentState)
    {
        if (!vds || !vds->frustumParamsBuf)
            break;

        auto& view = _views[vds->view->viewID];

        if (!vds->decalsBuf)
            buffersChanged = true;

        // Decal input buffer: keep room for all decals (+1 entry at index 0 for count).
        BufferAccess<DecalGPU> decals(vds->decalsBuf);

        auto currentDecalsCapacity = decals.capacity();
        if (currentDecalsCapacity > 0u)
            --currentDecalsCapacity;

        // Does the decals buffer need to grow?
        if (_totalNumDecals >= currentDecalsCapacity)
        {
            constexpr std::size_t growBy = 16u;

            const auto newDecalCapacity = (_totalNumDecals > currentDecalsCapacity + growBy) ?
                _totalNumDecals :
                (currentDecalsCapacity + growBy);

            Log()->debug("DecalSystemNode: resizing decals buffer to {} at {} bytes", newDecalCapacity, newDecalCapacity * sizeof(DecalGPU));

            // creates a new descriptor buffer, so we have to notify users to rebuild DSets that use it.
            decals.resize(newDecalCapacity + 1, vsgcontext);
            buffersChanged = true;
        }

        // See if the frustum grid has changed size, and if so, resize the decal tiles buffer to match.
        BufferAccess<FrustumGridParamsGPU> params(vds->frustumParamsBuf);
        auto numFrustumTiles = params->numTiles.x * params->numTiles.y;

        GPUOnlyBufferAccess<DecalTileGPU> decalTiles(vds->decalTilesBuf);

        auto numDecalTiles = decalTiles.capacity();
        if (numDecalTiles != numFrustumTiles)
        {
            Log()->debug("DecalSystemNode: resizing decal tiles buffer from {} to {} tiles at {} bytes", numDecalTiles, numFrustumTiles, numFrustumTiles * sizeof(DecalTileGPU));
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

    auto pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{
            vsg::DescriptorSetLayout::create(), // set 0 (empty)
            vds->descriptorSet->setLayout,      // set 1 (VDS)
        },
        vsg::PushConstantRanges{}
    );

    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, _cullingShader);

    auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

    auto bindVDS = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->layout,
        DESCRIPTOR_SET_VDS,
        vds->descriptorSet);

    // launches the compute shader:
    BufferAccess<FrustumGridParamsGPU> params(vds->frustumParamsBuf);
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

            BufferAccess<DecalGPU> gpudecal(vds->decalsBuf);
            const auto gpuCapacity = gpudecal.capacity();
            if (gpuCapacity == 0u)
                continue;

            std::uint32_t decalIndex = 0u;

            // TODO: why are we reading viewport 0 only?
            auto& vp = vds->viewportData->at(0);
            RenderingState rs{
                viewID, (FrameCountType)~0,
                { vp[0], vp[1], vp[2], vp[3] }
            };

            _registry.read([&](entt::registry& reg)
            {
                auto applyStyle = [&](entt::entity e_style, DecalGPU& out) -> bool
                {
                    // store the texture arena index if we have a texture; else -1 to indicate no texture.
                    out.color = StockColor::White;
                    out.textureIndex = -1;

                    if (reg.all_of<DecalStyle, DecalStyleDetail>(e_style))
                    {
                        auto& styleDecal = reg.get<DecalStyle>(e_style);
                        auto& styleDecalDetail = reg.get<DecalStyleDetail>(e_style);
                        out.color = styleDecal.color;
                        out.textureIndex = styleDecalDetail.descriptorImageIndex;
                        return true;
                    }

                    if (reg.all_of<Overlay, DecalStyleDetail>(e_style))
                    {
                        auto& styleOverlay = reg.get<Overlay>(e_style);
                        auto& styleOverlayDetail = reg.get<DecalStyleDetail>(e_style);
                        out.color = styleOverlay.color;
                        out.textureIndex = styleOverlayDetail.descriptorImageIndex;
                        return true;
                    }

                    return false;
                };

                auto applyProjection = [&](entt::entity e_optics, const glm::dmat4& mvm, bool requireOptics, DecalGPU& out) -> bool
                {
                    auto* optics = reg.try_get<Optics>(e_optics);
                    if (!optics)
                    {
                        if (requireOptics)
                            return false;

                        out.mvm = glm::fmat4(mvm);
                        out.distance = 0.0f;
                        out.zMin = 1.0f;
                        out.zMax = 10.0f;
                        out.cullingRadius = 1.0f;
                        out.tanHalfFovY = 0.0f;
                        out.aspect = 1.0f;
                        return true;
                    }

                    auto* opticsDetails = reg.try_get<OpticsDetail>(e_optics);
                    auto* opticsTransformDetail = reg.try_get<TransformDetail>(e_optics);
                    if (!opticsDetails || !opticsTransformDetail)
                        return false;

                    auto* opticsDetail = &opticsDetails->views[viewID];
                    auto& opticsTransformView = opticsTransformDetail->views[viewID];
                    if (opticsTransformView.revision < 0)
                        return false;

                    // Optics always project relative to the Transform on the Optics
                    // entity. This also makes an explicitly referenced Optics entity
                    // behave consistently for perspective and orthographic decals.
                    glm::dmat4 opticsModel = to_glm(opticsTransformView.model) * optics->pose;

                    if (optics->projection == Optics::Projection::Perspective)
                    {
                        // Strip scale from projector basis so metric near/far/focal values remain valid.
                        glm::dvec3 x(opticsModel[0]);
                        glm::dvec3 y(opticsModel[1]);
                        glm::dvec3 z(opticsModel[2]);

                        double lx = glm::length(x);
                        double ly = glm::length(y);
                        double lz = glm::length(z);
                        if (lx <= 0.0 || ly <= 0.0 || lz <= 0.0)
                            return false;

                        x /= lx;
                        y /= ly;
                        z /= lz;

                        opticsModel[0] = glm::dvec4(x, 0.0);
                        opticsModel[1] = glm::dvec4(y, 0.0);
                        opticsModel[2] = glm::dvec4(z, 0.0);

                        auto opticsMvm = vm * opticsModel;
                        out.mvm = glm::fmat4(opticsMvm);
                        // NB: gpudecal->mvmInverse is computed in the culling shader.

                        float tanH = tanf(glm::radians((float)optics->fovY * 0.5f));
                        float nearClip = std::max(1.0f, (float)opticsDetail->nearDistance);
                        float farClip = std::max(nearClip + 1.0f, (float)opticsDetail->farDistance);

                        float halfDepth = 0.5f * (farClip - nearClip);
                        float farHalfW = farClip * tanH * (float)optics->aspectRatio;
                        float farHalfH = farClip * tanH;
                        float bsRadius = sqrtf(farHalfW * farHalfW + farHalfH * farHalfH + halfDepth * halfDepth);

                        // Projector looks down local -Z; visible range is [-far, -near].
                        out.zMin = -farClip;
                        out.zMax = -nearClip;
                        out.cullingRadius = bsRadius;
                        out.distance = 1.0f; // perspective flag (>0)
                        out.tanHalfFovY = tanH;
                        out.aspect = (float)optics->aspectRatio;
                    }
                    else
                    {
                        // Orthographic projection uses the posed unit cube, including
                        // scale. Auto-clamping only replaces its world-space center.
                        if (optics->autoComputeFocalDistance && opticsDetail->focalPointValid)
                        {
                            opticsModel[3] = glm::dvec4(opticsDetail->focalPoint, 1.0);
                        }
                        out.mvm = glm::fmat4(vm * opticsModel);
                        // NB: gpudecal->mvmInverse is computed in the culling shader.
                        out.distance = 0.0f; // zero means orthographic
                    }

                    return true;
                };

                auto updateProjected = [&](auto entity, auto& projectedDetail, auto& active, auto& visibility, auto& transformDetail, entt::entity e_optics, entt::entity e_style, bool requireOptics, bool flipOverlayY, bool* outStyleApplied = nullptr)
                {
                    ROCKY_HARD_ASSERT(decalIndex + 1u < gpuCapacity, "DecalSystemNode: decal SSBO overflow");

                    if (!visible(visibility, rs))
                    {
                        return;
                    }

                    auto& transformView = transformDetail.views[viewID];
                    if (transformView.revision < 0)
                        return;

                    auto modelWorld = to_glm(transformView.model);
                    auto mvm = vm * modelWorld;

                    DecalGPU pending{};

                    if (!applyProjection(e_optics, mvm, requireOptics, pending))
                        return;

                    bool styleApplied = applyStyle(e_style, pending);

                    // first decal just holds the count, so advance first:
                    ++gpudecal;
                    ++decalIndex;

                    // update the detail record with the index of this decal in the SSBO,
                    // so we can find it later if we need to change the style
                    projectedDetail.ssboIndex = decalIndex;

                    gpudecal->mvm = pending.mvm;
                    gpudecal->mvmInverse = pending.mvmInverse;
                    gpudecal->color = pending.color;
                    gpudecal->textureIndex = pending.textureIndex;
                    gpudecal->distance = pending.distance;
                    gpudecal->zMin = pending.zMin;
                    gpudecal->zMax = pending.zMax;
                    gpudecal->cullingRadius = pending.cullingRadius;
                    gpudecal->tanHalfFovY = pending.tanHalfFovY;
                    gpudecal->aspect = pending.aspect;
                    gpudecal->_padding[0] = flipOverlayY ? 1 : 0;

                    if (outStyleApplied)
                        *outStyleApplied = styleApplied;
                };

                reg.view<Decal, DecalDetail, ActiveState, Visibility, TransformDetail>().each(
                    [&](auto entity, auto& decal, auto& detail, auto& active, auto& visibility, auto& transformDetail)
                    {
                        auto e_optics = decal.optics != entt::null ? decal.optics : decal.owner;
                        auto e_style = decal.style != entt::null ? decal.style : decal.owner;
                        bool requireOptics = decal.optics != entt::null;
                        updateProjected(entity, detail, active, visibility, transformDetail, e_optics, e_style, requireOptics, false);
                    });

                reg.view<Overlay, OverlayDetail, ActiveState, Visibility, TransformDetail>().each(
                    [&](auto entity, auto& overlay, auto& detail, auto& active, auto& visibility, auto& transformDetail)
                    {
                        if (reg.any_of<Decal>(entity))
                            return;

                        updateProjected(entity, detail, active, visibility, transformDetail, entity, entity, false, true);
                    });
            });

            // Update the decal count (kept in record #0):
            gpudecal.reset();
            gpudecal->count = decalIndex;
            gpudecal.dirty();
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
