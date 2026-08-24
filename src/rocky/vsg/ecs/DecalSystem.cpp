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
#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define DECAL_CULLING_SHADER "shaders/rocky.decal.cull.comp"

namespace
{
    struct LegacyDecalAdapter
    {
        bool ownsProjectedTexture = false;
    };

    struct LegacyOverlayProjectionAdapter
    {
        bool ownsProjectedTexture = false;
    };

    struct LegacyDecalStyleAdapter
    {
        bool ownsImageTexture = false;
    };

    struct TextureSlotDetail
    {
        vsg::ref_ptr<vsg::ImageInfo> texture;
        // where is this texture in the descriptorimage?
        std::int32_t descriptorImageIndex = -1;
    };
}

void DecalSystemNode::on_construct_Decal(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<LegacyDecalAdapter>(e);
    if (!r.any_of<ProjectedTexture>(e))
    {
        const auto& decal = r.get<Decal>(e);
        auto& projected = r.emplace<ProjectedTexture>(e);
        projected.texture = decal.style;
        projected.projector = decal.optics;
        projected.requireOptics = decal.optics != entt::null;
        adapter.ownsProjectedTexture = true;
    }
    Decal::dirty(r, e);
}

void DecalSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<LegacyOverlayProjectionAdapter>(e);
    if (!r.any_of<ProjectedTexture>(e))
    {
        auto& projected = r.emplace<ProjectedTexture>(e);
        projected.texture = e;
        projected.projector = e;
        projected.color = r.get<Overlay>(e).color;
        adapter.ownsProjectedTexture = true;
    }
    Overlay::dirty(r, e);
}

void DecalSystemNode::on_construct_DecalStyle(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<LegacyDecalStyleAdapter>(e);
    if (!r.any_of<ImageTexture>(e))
    {
        auto& imageTexture = r.emplace<ImageTexture>(e);
        imageTexture.image = r.get<DecalStyle>(e).image;
        adapter.ownsImageTexture = true;
    }
    DecalStyle::dirty(r, e);
}

void DecalSystemNode::on_construct_ProjectedTexture(entt::registry& r, entt::entity e)
{
    r.get<ProjectedTexture>(e).owner = e;
    (void)r.get_or_emplace<ActiveState>(e);
    (void)r.get_or_emplace<Visibility>(e);
}

void DecalSystemNode::on_construct_TextureResource(entt::registry& r, entt::entity e)
{
    r.get<TextureResource>(e).owner = e;
}

void DecalSystemNode::on_destroy_Decal(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<LegacyDecalAdapter>(e))
    {
        if (adapter->ownsProjectedTexture && r.any_of<ProjectedTexture>(e))
            r.remove<ProjectedTexture>(e);
        r.remove<LegacyDecalAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<LegacyOverlayProjectionAdapter>(e))
    {
        if (adapter->ownsProjectedTexture && r.any_of<ProjectedTexture>(e))
            r.remove<ProjectedTexture>(e);
        r.remove<LegacyOverlayProjectionAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_DecalStyle(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<LegacyDecalStyleAdapter>(e))
    {
        if (adapter->ownsImageTexture && r.any_of<ImageTexture>(e))
            r.remove<ImageTexture>(e);
        r.remove<LegacyDecalStyleAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_ProjectedTexture(entt::registry& r, entt::entity e)
{
}

void DecalSystemNode::on_destroy_TextureResource(entt::registry& r, entt::entity e)
{
    r.remove<TextureSlotDetail>(e);
}

void DecalSystemNode::on_destroy_TextureSlotDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<TextureSlotDetail>(e);
    if (detail.descriptorImageIndex >= 0)
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
            r.on_construct<ProjectedTexture>().connect<&DecalSystemNode::on_construct_ProjectedTexture>(*this);
            r.on_construct<TextureResource>().connect<&DecalSystemNode::on_construct_TextureResource>(*this);
            r.on_update<Decal>().connect<&DecalSystemNode::on_update_Decal>(*this);
            r.on_update<DecalStyle>().connect<&DecalSystemNode::on_update_DecalStyle>(*this);
            r.on_update<Overlay>().connect<&DecalSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Decal>().connect<&DecalSystemNode::on_destroy_Decal>(*this);
            r.on_destroy<DecalStyle>().connect<&DecalSystemNode::on_destroy_DecalStyle>(*this);
            r.on_destroy<Overlay>().connect<&DecalSystemNode::on_destroy_Overlay>(*this);
            r.on_destroy<ProjectedTexture>().connect<&DecalSystemNode::on_destroy_ProjectedTexture>(*this);
            r.on_destroy<TextureResource>().connect<&DecalSystemNode::on_destroy_TextureResource>(*this);
            r.on_destroy<TextureSlotDetail>().connect<&DecalSystemNode::on_destroy_TextureSlotDetail>(*this);

            // Set up the dirty tracking
            auto e = r.create();
            r.emplace<Decal::Dirty>(e);
            r.emplace<DecalStyle::Dirty>(e);
            r.emplace<Overlay::Dirty>(e);
            r.emplace<ProjectedTexture::Dirty>(e);

            // Normalize facade components that predate this system.
            std::vector<entt::entity> existingStyles;
            r.view<DecalStyle>().each([&](auto entity, auto&) { existingStyles.push_back(entity); });
            for (auto entity : existingStyles)
                on_construct_DecalStyle(r, entity);

            std::vector<entt::entity> existingDecals;
            r.view<Decal>().each([&](auto entity, auto&) { existingDecals.push_back(entity); });
            for (auto entity : existingDecals)
                on_construct_Decal(r, entity);

            std::vector<entt::entity> existingOverlays;
            r.view<Overlay>().each([&](auto entity, auto&) { existingOverlays.push_back(entity); });
            for (auto entity : existingOverlays)
                on_construct_Overlay(r, entity);

            // Seed normalized detail for low-level components that predate this system.
            r.view<ProjectedTexture>().each([&](auto entity, auto& projected)
                {
                    projected.owner = entity;
                    (void)r.get_or_emplace<ActiveState>(entity);
                    (void)r.get_or_emplace<Visibility>(entity);
                });

            r.view<TextureResource>().each([&](auto entity, auto& resource)
                {
                    resource.owner = entity;
                });
        });
}

void
DecalSystemNode::updateStyles(VSGContext vsgcontext)
{
    auto writer = _registry.write();
    auto& reg = writer.registry;
    bool sharedDescriptorsDirty = false;

    auto textures = vsgcontext->sharedRenderData->decalTextures;
    if (!textures || textures->imageInfoList.empty())
        return;
    if (!_fallbackTexture)
        _fallbackTexture = textures->imageInfoList[0];
    auto fallback = _fallbackTexture;

    // A DecalStyle is now just a legacy ImageTexture facade.
    reg.view<DecalStyle, LegacyDecalStyleAdapter, ImageTexture>().each(
        [&](auto, auto& style, auto& adapter, auto& imageTexture)
        {
            if (adapter.ownsImageTexture)
                imageTexture.image = style.image;
        });

    // Component destruction cannot safely edit the descriptor arena, so return
    // queued slots here. TextureResource owns its ImageInfo; this consumer only
    // relinquishes the descriptor reference and never disposes producer data.
    {
        std::vector<std::int32_t> pendingSlots;
        {
            std::scoped_lock lock(_pendingTextureSlotsMutex);
            pendingSlots.swap(_pendingTextureSlots);
        }
        std::sort(pendingSlots.begin(), pendingSlots.end());
        pendingSlots.erase(std::unique(pendingSlots.begin(), pendingSlots.end()), pendingSlots.end());
        for (auto slot : pendingSlots)
        {
            if (slot >= 0 && slot < static_cast<std::int32_t>(textures->imageInfoList.size()))
            {
                textures->imageInfoList[slot] = fallback;
                sharedDescriptorsDirty = true;
            }
        }
    }

    auto releaseSlot = [&](TextureSlotDetail& detail)
    {
        if (detail.descriptorImageIndex >= 0 &&
            detail.descriptorImageIndex < static_cast<std::int32_t>(textures->imageInfoList.size()))
        {
            textures->imageInfoList[detail.descriptorImageIndex] = fallback;
            sharedDescriptorsDirty = true;
        }
        detail.descriptorImageIndex = -1;
        detail.texture = {};
    };

    std::size_t rejectedTextureCount = 0u;

    auto assignResource = [&](const TextureResource& resource, TextureSlotDetail& detail)
    {
        if (!resource.ready || !resource.texture)
        {
            if (detail.descriptorImageIndex >= 0 || detail.texture)
                releaseSlot(detail);
            return;
        }

        if (detail.texture == resource.texture && detail.descriptorImageIndex >= 0 &&
            detail.descriptorImageIndex < static_cast<std::int32_t>(textures->imageInfoList.size()) &&
            textures->imageInfoList[detail.descriptorImageIndex] == resource.texture)
            return;

        int slot = detail.descriptorImageIndex;
        if (slot < 0)
        {
            for (std::size_t i = 0; i < textures->imageInfoList.size(); ++i)
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
            ++rejectedTextureCount;
            releaseSlot(detail);
            return;
        }

        textures->imageInfoList[slot] = resource.texture;
        detail.descriptorImageIndex = slot;
        detail.texture = resource.texture;
        requestCompile(resource.texture);
        sharedDescriptorsDirty = true;
    };

    // The descriptor arena is shared by all views, but it only needs textures
    // referenced by projections that were visible in a recently recorded frame.
    // Node-paged data can remain active in the registry after leaving the view;
    // retaining all of those textures quickly exhausts the arena while panning.
    const auto frame = vsgcontext->viewer()->getFrameStamp()->frameCount;
    auto visibleInAnyActiveView = [&](const Visibility& visibility)
    {
        for (auto viewID : vsgcontext->activeViewIDs)
        {
            if (viewID < ROCKY_MAX_NUMBER_OF_VIEWS)
            {
                RenderingState rs{ viewID, frame, {} };
                if (visible(visibility, rs))
                    return true;
            }
        }
        return false;
    };

    std::unordered_set<entt::entity> demandedTextures;
    reg.view<ProjectedTexture, ActiveState, Visibility>().each(
        [&](auto entity, auto& projected, auto&, auto& visibility)
        {
            if (visibleInAnyActiveView(visibility))
                demandedTextures.insert(projected.texture != entt::null ? projected.texture : entity);
        });

    reg.view<TextureSlotDetail>().each([&](auto entity, auto& detail)
        {
            if (demandedTextures.find(entity) == demandedTextures.end())
                releaseSlot(detail);
        });

    for (auto entity : demandedTextures)
    {
        if (!reg.valid(entity))
            continue;
        if (auto* resource = reg.try_get<TextureResource>(entity))
        {
            auto& detail = reg.get_or_emplace<TextureSlotDetail>(entity);
            assignResource(*resource, detail);
        }
    }

    if (rejectedTextureCount > 0u)
    {
        if (!_textureSlotsExhausted)
        {
            Log()->warn(
                "DecalSystemNode: {} visible projected textures exceed the {} available descriptor slots; some will not render",
                demandedTextures.size(), textures->imageInfoList.size());
        }
        _textureSlotsExhausted = true;
    }
    else
    {
        _textureSlotsExhausted = false;
    }

    if (sharedDescriptorsDirty)
    {
        requestCompile(textures);
        vsgcontext->sharedRenderData->dirtySharedDescriptors();
    }

    // Drain legacy dirty queues; normalized contracts are polled by identity and
    // revision and therefore support multiple independent consumers.
    DecalStyle::eachDirty(reg, [](entt::entity) {});
    Overlay::eachDirty(reg, [](entt::entity) {});
    Decal::eachDirty(reg, [](entt::entity) {});
}

void
DecalSystemNode::resizeGPUBuffersIfNeeded(VSGContext vsgcontext)
{
    bool buffersChanged = false;
    unsigned totalNumDecals = 0u;

    // Recount from registry to keep capacity decisions in sync with actual entities.
    _registry.read([&](entt::registry& reg)
    {
        auto projections = reg.view<ProjectedTexture, ActiveState, Visibility>();
        projections.each([&](auto, auto&, auto&, auto&) { ++totalNumDecals; });
    });

    for (auto& vds : _sharedRenderData->viewDependentState)
    {
        if (!vds || !vds->frustumParamsBuf)
            continue;

        auto& view = _views[vds->view->viewID];

        if (!vds->decalsBuf)
            buffersChanged = true;

        // Decal input buffer: keep room for all decals (+1 entry at index 0 for count).
        BufferAccess<DecalGPU> decals(vds->decalsBuf);

        auto currentDecalsCapacity = decals.capacity();
        if (currentDecalsCapacity > 0u)
            --currentDecalsCapacity;

        // Does the decals buffer need to grow?
        if (totalNumDecals >= currentDecalsCapacity)
        {
            constexpr std::size_t growBy = 16u;

            const auto newDecalCapacity = (totalNumDecals > currentDecalsCapacity + growBy) ?
                totalNumDecals :
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
                auto applyTexture = [&](entt::entity e_texture, const Color& color, DecalGPU& out, std::int32_t& flags) -> bool
                {
                    out.color = color;
                    out.textureIndex = -1;
                    flags = 0;

                    if (auto* resource = reg.try_get<TextureResource>(e_texture))
                    {
                        if (!resource->ready)
                            return false;
                        if (resource->texture)
                        {
                            auto* textureDetail = reg.try_get<TextureSlotDetail>(e_texture);
                            if (!textureDetail || textureDetail->descriptorImageIndex < 0)
                                return false;
                            out.textureIndex = textureDetail->descriptorImageIndex;
                            if (resource->origin == TextureOrigin::UpperLeft)
                                flags |= 1;
                            if (resource->alphaMode == TextureAlphaMode::Premultiplied)
                                flags |= 2;
                        }
                    }

                    return true;
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
                        // The inverse is computed once on the CPU below.

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
                        bool recenter = optics->autoComputeFocalDistance;
                        if (auto* terrainClamp = reg.try_get<TerrainClamp>(e_optics))
                            recenter = terrainClamp->enabled && terrainClamp->recenterOrthographic;
                        if (recenter && opticsDetail->focalPointValid)
                        {
                            opticsModel[3] = glm::dvec4(opticsDetail->focalPoint, 1.0);
                        }
                        out.mvm = glm::fmat4(vm * opticsModel);
                        // The inverse is computed once on the CPU below.
                        out.distance = 0.0f; // zero means orthographic
                    }

                    return true;
                };

                auto updateProjected = [&](auto entity, auto& projected, auto&, auto& visibility)
                {
                    ROCKY_HARD_ASSERT(decalIndex + 1u < gpuCapacity, "DecalSystemNode: decal SSBO overflow");

                    if (!visible(visibility, rs))
                    {
                        return;
                    }

                    auto e_projector = projected.projector != entt::null ? projected.projector : entity;
                    auto e_texture = projected.texture != entt::null ? projected.texture : entity;
                    auto* transformDetail = reg.try_get<TransformDetail>(e_projector);
                    if (!transformDetail)
                        return;

                    auto& transformView = transformDetail->views[viewID];
                    if (transformView.revision < 0)
                        return;

                    auto modelWorld = to_glm(transformView.model);
                    auto mvm = vm * modelWorld;

                    DecalGPU pending{};

                    if (!applyProjection(e_projector, mvm, projected.requireOptics, pending))
                        return;

                    if (pending.distance == 0.0f)
                    {
                        auto* adapter = reg.try_get<LegacyDecalAdapter>(entity);
                        auto* style = reg.try_get<DecalStyle>(e_texture);
                        if (adapter && adapter->ownsProjectedTexture && style && style->textureSize)
                        {
                            const auto dimensions = glm::abs(*style->textureSize);
                            const float xLength = glm::length(glm::fvec3(pending.mvm[0]));
                            const float yLength = glm::length(glm::fvec3(pending.mvm[1]));
                            if (dimensions.x > 0.0 && xLength > 0.0f)
                                pending.mvm[0] *= static_cast<float>(dimensions.x) / xLength;
                            if (dimensions.y > 0.0 && yLength > 0.0f)
                                pending.mvm[1] *= static_cast<float>(dimensions.y) / yLength;
                        }
                    }

                    pending.mvmInverse = glm::inverse(pending.mvm);

                    std::int32_t textureFlags = 0;
                    if (!applyTexture(e_texture, projected.color, pending, textureFlags))
                        return;

                    // first decal just holds the count, so advance first:
                    ++gpudecal;
                    ++decalIndex;

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
                    gpudecal->_padding[0] = textureFlags;
                };

                reg.view<ProjectedTexture, ActiveState, Visibility>().each(
                    [&](auto entity, auto& projected, auto& active, auto& visibility)
                    {
                        updateProjected(entity, projected, active, visibility);
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

    _registry.write([&](entt::registry& r)
        {
            // Normalize legacy public components every frame. This deliberately
            // supports the established pattern of editing fields by reference.
            r.view<Decal, LegacyDecalAdapter, ProjectedTexture>().each(
                [&](auto entity, auto& decal, auto& adapter, auto& projected)
                {
                    if (!adapter.ownsProjectedTexture)
                        return;
                    projected.texture = decal.style != entt::null ? decal.style : entity;
                    projected.projector = decal.optics != entt::null ? decal.optics : entity;
                    projected.requireOptics = decal.optics != entt::null;
                    projected.color = StockColor::White;
                    if (auto* style = r.try_get<DecalStyle>(projected.texture))
                        projected.color = style->color;
                });

            r.view<Overlay, LegacyOverlayProjectionAdapter, ProjectedTexture>().each(
                [&](auto entity, auto& overlay, auto& adapter, auto& projected)
                {
                    if (!adapter.ownsProjectedTexture)
                        return;
                    projected.texture = entity;
                    projected.projector = entity;
                    projected.color = overlay.color;
                    projected.requireOptics = false;
                });
        });

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
