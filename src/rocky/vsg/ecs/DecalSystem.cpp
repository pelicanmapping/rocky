/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include "DecalSystem.h"
#include "OpticsSystem.h"
#include "ECSVisitors.h"
#include "OverlayRenderContext.h"
#ifdef ROCKY_HAS_SLUGHORN
#include "SlugResource.h"
#endif
#include "../ViewDependentState.h"
#include "../SharedRenderData.h"
#include "../ShaderDefines.h"
#include <rocky/ecs/Optics.h>
#include <rocky/vsg/Application.h>
#include <rocky/vsg/VSGUtils.h>
#include <vsg/vk/State.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::detail;

#define DECAL_CULLING_SHADER "shaders/rocky.decal.cull.comp"

namespace ROCKY_NAMESPACE::detail
{
    //! Submission snapshot: includes raster volumes for the shared cell limit, but only vectors have an owner.
    struct VectorOverlayPickRecord
    {
        DecalGPU decal;
        glm::dmat4 worldToLocal{ 1.0 };
        entt::entity entity = entt::null;
        entt::entity payload = entt::null;
        std::uint64_t revision = 0u;
    };

    //! Reused per-view storage; updated and queried serially on the application thread.
    struct VectorOverlayPickView
    {
        FrustumGridParamsGPU grid;
        VkViewport viewport{};
        glm::dmat4 viewMatrix{ 1.0 };
        glm::dmat4 projectionMatrix{ 1.0 };
        std::vector<VectorOverlayPickRecord> records;
    };

    //! Reproduces the compute shader's cell frustum in float precision, including viewport offsets.
    //! Returns false outside the viewport or before the grid is ready.
    static bool pickCellBounds(const FrustumGridParamsGPU& grid, int x, int y, glm::vec4& bounds)
    {
        const glm::ivec2 pixel = glm::ivec2(x, y) - glm::ivec2(grid.viewport);
        if (grid.viewport.z <= 0 || grid.viewport.w <= 0 || grid.pixelsPerTile == 0u ||
            pixel.x < 0 || pixel.y < 0 || pixel.x >= grid.viewport.z || pixel.y >= grid.viewport.w)
            return false;
        const glm::uvec2 tile = glm::uvec2(pixel) / unsigned(FRUSTUM_GRID_TILE_SIZE_PIXELS);
        if (tile.x >= grid.numTiles.x || tile.y >= grid.numTiles.y)
            return false;
        const glm::vec2 lo = glm::vec2(tile * grid.pixelsPerTile);
        const glm::vec2 hi = glm::min(lo + float(grid.pixelsPerTile), glm::vec2(grid.viewport.z, grid.viewport.w));
        for (unsigned i = 0; i < 4; ++i)
        {
            const glm::vec2 uv = glm::vec2(i & 1u ? hi.x : lo.x, i & 2u ? hi.y : lo.y) /
                glm::vec2(grid.viewport.z, grid.viewport.w);
            auto v = grid.invProjMatrix * glm::vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
            v /= v.w;
            const glm::vec2 q = grid.projIsOrtho ? glm::vec2(v) : glm::vec2(v) / -v.z;
            if (!std::isfinite(q.x) || !std::isfinite(q.y))
                return false;
            if (i == 0u)
                bounds = { q.x, q.x, q.y, q.y };
            else
                bounds = { std::min(bounds.x, q.x), std::max(bounds.y, q.x),
                    std::min(bounds.z, q.y), std::max(bounds.w, q.y) };
        }
        return true;
    }

    //! Mirrors rocky.decal.cull.comp, so nonpickable raster volumes still consume their submitted cell slots.
    static bool pickCellIntersects(const DecalGPU& decal, const glm::vec4& bounds, bool ortho)
    {
        const auto& m = decal.mvm;
        const bool sphere = decal.distance > 0.0f;
        const glm::vec3 center = sphere ? glm::vec3(m * glm::vec4(0, 0, (decal.zMin + decal.zMax) * 0.5f, 1)) :
            glm::vec3(m[3]);
        const glm::vec3 axes[] = { glm::vec3(m[0]) * 0.5f, glm::vec3(m[1]) * 0.5f, glm::vec3(m[2]) * 0.5f };
        const glm::vec3 normals[] = {
            { 1, 0, ortho ? 0.0f : bounds.x }, { -1, 0, ortho ? 0.0f : -bounds.y },
            { 0, 1, ortho ? 0.0f : bounds.z }, { 0, -1, ortho ? 0.0f : -bounds.w } };
        const float offsets[] = { bounds.x, -bounds.y, bounds.z, -bounds.w };
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto& n = normals[i];
            const float radius = sphere ? decal.cullingRadius * glm::length(n) :
                std::abs(glm::dot(n, axes[0])) + std::abs(glm::dot(n, axes[1])) + std::abs(glm::dot(n, axes[2]));
            if (glm::dot(n, center) - (ortho ? offsets[i] : 0.0f) < -radius)
                return false;
        }
        return true;
    }

    //! Intersects a pixel ray with the receiver's tangent plane in double precision.
    //! Finite interior clip depths support reversed depth and infinite-far perspective cameras.
    static bool pickPlanePoint(const glm::dmat4& inverseVP, const VkViewport& vp, double x, double y,
        const glm::dvec3& origin, const glm::dvec3& normal, glm::dvec3& out)
    {
        const double nx = (x - vp.x) / vp.width * 2.0 - 1.0;
        const double ny = (y - vp.y) / vp.height * 2.0 - 1.0;
        auto a = inverseVP * glm::dvec4(nx, ny, 0.25, 1.0);
        auto b = inverseVP * glm::dvec4(nx, ny, 0.75, 1.0);
        if (a.w == 0.0 || b.w == 0.0)
            return false;
        const glm::dvec3 start = glm::dvec3(a) / a.w;
        const glm::dvec3 direction = glm::dvec3(b) / b.w - start;
        const double denominator = glm::dot(normal, direction);
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1e-12 * glm::length(direction))
            return false;
        out = start + direction * (glm::dot(normal, origin - start) / denominator);
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
    }

    //! Main-pass-only diagnostic renderer. Two reusable pipeline variants share the existing per-view
    //! decal buffer; a procedural 12-edge box needs no per-decal geometry or copied transforms.
    //! The weak render-data reference avoids a cycle through its views and their scene graph.
    class DecalDebugNode : public vsg::Inherit<vsg::Group, DecalDebugNode>
    {
    public:
        bool enabled = false;
        bool seeThrough = true;
        std::weak_ptr<SharedRenderData> renderData;

        //! Create depth-tested and see-through pipelines before the scene's normal compile traversal.
        //! Missing shaders disable only this diagnostic, not decal rendering. Reinitialization defers disposal.
        void initialize(VSGContext context)
        {
            for (auto& child : children)
                context->dispose(child);
            children.clear();
            renderData = context->sharedRenderData;

            auto vertex = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main",
                vsg::findFile("shaders/rocky.decal.debug.vert", context->searchPaths), context->readerWriterOptions);
            auto fragment = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main",
                vsg::findFile("shaders/rocky.decal.debug.frag", context->searchPaths), context->readerWriterOptions);
            if (!vertex || !fragment)
            {
                Log()->warn("DecalSystemNode: missing decal volume debug shaders");
                return;
            }

            auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{ vertex, fragment });
            addViewDependentStateToShaderSet(shaderSet);
            shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

            for (unsigned xray = 0; xray != 2; ++xray)
            {
                auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);
                config->shaderHints = context->shaderCompileSettings;
                enableViewDependentStateUniforms(config);
                for (auto& state : config->pipelineStates)
                {
                    if (auto* assembly = dynamic_cast<vsg::InputAssemblyState*>(state.get()))
                        assembly->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                    if (auto* raster = dynamic_cast<vsg::RasterizationState*>(state.get()))
                        raster->cullMode = VK_CULL_MODE_NONE;
                    if (auto* depth = dynamic_cast<vsg::DepthStencilState*>(state.get()))
                    {
                        depth->depthTestEnable = xray ? VK_FALSE : VK_TRUE;
                        depth->depthWriteEnable = VK_FALSE;
                    }
                }
                config->init();
                auto group = vsg::StateGroup::create();
                group->add(config->bindGraphicsPipeline);
                group->add(vsg::BindViewDescriptorSets::create(
                    VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, DESCRIPTOR_SET_VDS));
                addChild(group);
            }
        }

        //! Record one instanced line draw for the current main view. Read only the CPU-owned count;
        //! the vertex shader fetches final GPU transforms, bypassing the capped Forward+ cell lists.
        void traverse(vsg::RecordTraversal& record) const override
        {
            if (!enabled || children.size() != 2 ||
                getRenderRequest(record).purpose != RenderPurpose::Main)
                return;

            auto shared = renderData.lock();
            auto* commandBuffer = record.getCommandBuffer();
            if (!shared || !commandBuffer || commandBuffer->viewID >= ROCKY_MAX_NUMBER_OF_VIEWS)
                return;
            auto& vds = shared->viewDependentState[commandBuffer->viewID];
            // Shadow and offscreen views must not borrow the main camera's view-space transforms.
            if (!vds || commandBuffer->viewDependentState != vds.get() || !vds->decalsBuf)
                return;

            BufferAccess<DecalGPU> decals(vds->decalsBuf);
            if (decals.capacity() <= 1u || decals->count <= 0)
                return;
            auto count = static_cast<uint32_t>(std::min<std::size_t>(decals->count, decals.capacity() - 1u));

            auto* group = static_cast<vsg::StateGroup*>(children[seeThrough ? 1 : 0].get());
            auto* state = record.getState();
            state->push(group->stateCommands);
            // Stack-local command avoids shared mutable draw counts when views record concurrently.
            // firstInstance skips element zero, the decal-count header.
            vsg::Draw draw(24u, count, 0u, 1u);
            draw.accept(record);
            state->pop(group->stateCommands);
        }
    };

    /*
     * The decal renderer consumes a small, composable component model:
     *
     *   ImageTexture/RenderTexture -> TextureResource -> ProjectedTexture
     *   Slug Overlay              -> SlugResource    -> ProjectedTexture
     *
     * Decal, DecalStyle, and Overlay are convenience facades over that model.
     * The private FacadeAdapter components below record which low-level
     * components this system synthesized for a facade. "Adapter" is not a
     * deprecation marker; it is ownership bookkeeping that prevents facade
     * destruction from removing a caller-supplied low-level component.
     */

    //! Records that DecalSystem materialized a ProjectedTexture for a Decal.
    //! The generated projection references Decal::style as its payload and
    //! Decal::optics as its projector.
    struct DecalFacadeAdapter
    {
        bool ownsProjectedTexture = false;
    };

    //! Records that DecalSystem materialized a self-projecting
    //! ProjectedTexture for an Overlay. OverlayBakeSystem or SlugSystem then
    //! supplies the payload resource selected by Overlay::mode.
    struct OverlayProjectionFacadeAdapter
    {
        bool ownsProjectedTexture = false;
    };

    //! Records that DecalSystem materialized an ImageTexture for a DecalStyle.
    //! TextureSystem subsequently turns that normalized source into a
    //! TextureResource.
    struct DecalStyleFacadeAdapter
    {
        bool ownsImageTexture = false;
    };

#ifdef ROCKY_HAS_SLUGHORN
    /**
     * Consumer-side binding and publication state for one SlugResource.
     *
     * A Slug atlas is a matched curve/band texture pair, so both arrays always
     * use descriptorImageIndex. resourceRevision says which published atlas is
     * installed. descriptorWriteFrame/readyForDraw impose a one-frame barrier:
     * TerrainNode rebuilds the global descriptor set before ECS updates, so
     * DecalSystem must not expose new shape metadata until a later frame can
     * prove the matching descriptors have been installed.
     */
    struct SlugSlotDetail
    {
        vsg::ref_ptr<vsg::ImageInfo> curveTexture;
        vsg::ref_ptr<vsg::ImageInfo> bandTexture;
        std::int32_t descriptorImageIndex = -1;
        std::uint64_t resourceRevision = 0u;
        std::uint64_t descriptorWriteFrame = 0u;
        bool readyForDraw = false;
    };
#endif
}

// Facade construction is deliberately non-destructive. If the normalized
// component already exists, it belongs to the caller and the adapter leaves
// its ownership flag false. The matching destruction callbacks remove only
// components that were synthesized here.
void DecalSystemNode::on_construct_Decal(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<DecalFacadeAdapter>(e);
    if (!r.any_of<ProjectedTexture>(e))
    {
        const auto& decal = r.get<Decal>(e);
        auto& projected = r.emplace<ProjectedTexture>(e);
        projected.texture = decal.style;
        projected.projector = decal.optics;
        projected.placement = decal.placement;
        projected.computeClipRange = decal.computeClipRange;
        projected.requireOptics = decal.optics != entt::null;
        adapter.ownsProjectedTexture = true;
    }
    Decal::dirty(r, e);
}

void DecalSystemNode::on_construct_Overlay(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<OverlayProjectionFacadeAdapter>(e);
    if (!r.any_of<ProjectedTexture>(e))
    {
        auto& projected = r.emplace<ProjectedTexture>(e);
        projected.texture = e;
        projected.projector = e;
        projected.color = r.get<Overlay>(e).color;
        projected.placement = ProjectionPlacement::Terrain;
        projected.computeClipRange = true;
        adapter.ownsProjectedTexture = true;
    }
    Overlay::dirty(r, e);
}

void DecalSystemNode::on_construct_DecalStyle(entt::registry& r, entt::entity e)
{
    auto& adapter = r.get_or_emplace<DecalStyleFacadeAdapter>(e);
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

#ifdef ROCKY_HAS_SLUGHORN
void DecalSystemNode::on_construct_SlugResource(entt::registry& r, entt::entity e)
{
    r.get<SlugResource>(e).owner = e;
}
#endif

void DecalSystemNode::on_destroy_Decal(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<DecalFacadeAdapter>(e))
    {
        if (adapter->ownsProjectedTexture && r.any_of<ProjectedTexture>(e))
            r.remove<ProjectedTexture>(e);
        r.remove<DecalFacadeAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_Overlay(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<OverlayProjectionFacadeAdapter>(e))
    {
        if (adapter->ownsProjectedTexture && r.any_of<ProjectedTexture>(e))
            r.remove<ProjectedTexture>(e);
        r.remove<OverlayProjectionFacadeAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_DecalStyle(entt::registry& r, entt::entity e)
{
    if (auto* adapter = r.try_get<DecalStyleFacadeAdapter>(e))
    {
        if (adapter->ownsImageTexture && r.any_of<ImageTexture>(e))
            r.remove<ImageTexture>(e);
        r.remove<DecalStyleFacadeAdapter>(e);
    }
}

void DecalSystemNode::on_destroy_ProjectedTexture(entt::registry& r, entt::entity e)
{
}

void DecalSystemNode::on_destroy_TextureResource(entt::registry& r, entt::entity e)
{
    r.remove<TextureSlotDetail>(e);
}

#ifdef ROCKY_HAS_SLUGHORN
void DecalSystemNode::on_destroy_SlugResource(entt::registry& r, entt::entity e)
{
    r.remove<SlugSlotDetail>(e);
}
#endif

void DecalSystemNode::on_destroy_TextureSlotDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<TextureSlotDetail>(e);
    if (detail.descriptorImageIndex >= 0)
    {
        std::scoped_lock lock(_pendingTextureSlotsMutex);
        _pendingTextureSlots.push_back(detail.descriptorImageIndex);
    }
}

#ifdef ROCKY_HAS_SLUGHORN
void DecalSystemNode::on_destroy_SlugSlotDetail(entt::registry& r, entt::entity e)
{
    auto& detail = r.get<SlugSlotDetail>(e);
    if (detail.descriptorImageIndex >= 0)
    {
        std::scoped_lock lock(_pendingTextureSlotsMutex);
        _pendingSlugSlots.push_back(detail.descriptorImageIndex);
    }
}
#endif

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
    Inherit(registry),
    _debugNode(DecalDebugNode::create())
{
    _registry.write([&](entt::registry& r)
        {
            // Install normalization, ownership, and descriptor-release hooks
            // for both facade and low-level decal components.
            r.on_construct<Decal>().connect<&DecalSystemNode::on_construct_Decal>(*this);
            r.on_construct<DecalStyle>().connect<&DecalSystemNode::on_construct_DecalStyle>(*this);
            r.on_construct<Overlay>().connect<&DecalSystemNode::on_construct_Overlay>(*this);
            r.on_construct<ProjectedTexture>().connect<&DecalSystemNode::on_construct_ProjectedTexture>(*this);
            r.on_construct<TextureResource>().connect<&DecalSystemNode::on_construct_TextureResource>(*this);
#ifdef ROCKY_HAS_SLUGHORN
            r.on_construct<SlugResource>().connect<&DecalSystemNode::on_construct_SlugResource>(*this);
#endif
            r.on_update<Decal>().connect<&DecalSystemNode::on_update_Decal>(*this);
            r.on_update<DecalStyle>().connect<&DecalSystemNode::on_update_DecalStyle>(*this);
            r.on_update<Overlay>().connect<&DecalSystemNode::on_update_Overlay>(*this);
            r.on_destroy<Decal>().connect<&DecalSystemNode::on_destroy_Decal>(*this);
            r.on_destroy<DecalStyle>().connect<&DecalSystemNode::on_destroy_DecalStyle>(*this);
            r.on_destroy<Overlay>().connect<&DecalSystemNode::on_destroy_Overlay>(*this);
            r.on_destroy<ProjectedTexture>().connect<&DecalSystemNode::on_destroy_ProjectedTexture>(*this);
            r.on_destroy<TextureResource>().connect<&DecalSystemNode::on_destroy_TextureResource>(*this);
#ifdef ROCKY_HAS_SLUGHORN
            r.on_destroy<SlugResource>().connect<&DecalSystemNode::on_destroy_SlugResource>(*this);
#endif
            r.on_destroy<TextureSlotDetail>().connect<&DecalSystemNode::on_destroy_TextureSlotDetail>(*this);
#ifdef ROCKY_HAS_SLUGHORN
            r.on_destroy<SlugSlotDetail>().connect<&DecalSystemNode::on_destroy_SlugSlotDetail>(*this);
#endif

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

#ifdef ROCKY_HAS_SLUGHORN
            r.view<SlugResource>().each([&](auto entity, auto& resource)
                {
                    resource.owner = entity;
                });
#endif
        });
}

void
DecalSystemNode::updateStyles(VSGContext vsgcontext)
{
    auto writer = _registry.write();
    auto& reg = writer.registry;
    bool sharedDescriptorsDirty = false;

    auto textures = vsgcontext->sharedRenderData->decalTextures;
#ifdef ROCKY_HAS_SLUGHORN
    auto slugCurves = vsgcontext->sharedRenderData->slugCurveTexture;
    auto slugBands = vsgcontext->sharedRenderData->slugBandTexture;
    if (!textures || textures->imageInfoList.empty() ||
        !slugCurves || slugCurves->imageInfoList.empty() ||
        !slugBands || slugBands->imageInfoList.empty() ||
        slugCurves->imageInfoList.size() != slugBands->imageInfoList.size())
        return;
#else
    if (!textures || textures->imageInfoList.empty())
        return;
#endif
    if (!_fallbackTexture)
        _fallbackTexture = textures->imageInfoList[0];
#ifdef ROCKY_HAS_SLUGHORN
    if (!_fallbackSlugCurve)
        _fallbackSlugCurve = slugCurves->imageInfoList[0];
    if (!_fallbackSlugBand)
        _fallbackSlugBand = slugBands->imageInfoList[0];
#endif
    auto fallback = _fallbackTexture;

    // Keep the facade-owned ImageTexture synchronized with DecalStyle. A
    // caller-owned ImageTexture is intentionally left alone.
    reg.view<DecalStyle, DecalStyleFacadeAdapter, ImageTexture>().each(
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

#ifdef ROCKY_HAS_SLUGHORN
    {
        std::vector<std::int32_t> pendingSlots;
        {
            std::scoped_lock lock(_pendingTextureSlotsMutex);
            pendingSlots.swap(_pendingSlugSlots);
        }
        std::sort(pendingSlots.begin(), pendingSlots.end());
        pendingSlots.erase(std::unique(pendingSlots.begin(), pendingSlots.end()), pendingSlots.end());
        for (auto slot : pendingSlots)
        {
            if (slot >= 0 && slot < static_cast<std::int32_t>(slugCurves->imageInfoList.size()))
            {
                slugCurves->imageInfoList[slot] = _fallbackSlugCurve;
                slugBands->imageInfoList[slot] = _fallbackSlugBand;
                sharedDescriptorsDirty = true;
            }
        }
    }
#endif

    auto releaseSlot = [&](TextureSlotDetail& detail)
    {
        if (detail.descriptorImageIndex >= 0 &&
            detail.descriptorImageIndex < static_cast<std::int32_t>(textures->imageInfoList.size()))
        {
            textures->imageInfoList[detail.descriptorImageIndex] = fallback;
            sharedDescriptorsDirty = true;
        }
        detail = {};
    };

#ifdef ROCKY_HAS_SLUGHORN
    auto releaseSlugSlot = [&](SlugSlotDetail& detail)
    {
        if (detail.descriptorImageIndex >= 0 &&
            detail.descriptorImageIndex < static_cast<std::int32_t>(slugCurves->imageInfoList.size()))
        {
            slugCurves->imageInfoList[detail.descriptorImageIndex] = _fallbackSlugCurve;
            slugBands->imageInfoList[detail.descriptorImageIndex] = _fallbackSlugBand;
            sharedDescriptorsDirty = true;
        }
        detail = {};
    };
#endif

    std::size_t rejectedTextureCount = 0u;
    const auto frame = vsgcontext->viewer()->getFrameStamp()->frameCount;

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
        {
            // As with Vector slots, the next terrain update must install this
            // image in the descriptor set before its index reaches the SSBO.
            if (!detail.readyForDraw && frame > detail.descriptorWriteFrame)
                detail.readyForDraw = true;
            return;
        }

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
        detail.descriptorWriteFrame = frame;
        detail.readyForDraw = false;
        requestCompile(resource.texture);
        sharedDescriptorsDirty = true;
    };

    // The descriptor arena is shared by all views, but it only needs textures
    // referenced by projections that were visible in a recently recorded frame.
    // Node-paged data can remain active in the registry after leaving the view;
    // retaining all of those textures quickly exhausts the arena while panning.
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
#ifdef ROCKY_HAS_SLUGHORN
    std::unordered_set<entt::entity> demandedSlugAtlases;
#endif
    reg.view<ProjectedTexture, ActiveState, Visibility>().each(
        [&](auto entity, auto& projected, auto&, auto& visibility)
        {
            if (!visibleInAnyActiveView(visibility))
                return;

            const auto payload = projected.texture != entt::null ? projected.texture : entity;
#ifdef ROCKY_HAS_SLUGHORN
            const auto* overlay = reg.try_get<Overlay>(payload);
            if (overlay && resolveOverlayMode(reg, payload, overlay->mode) == OverlayMode::Vector)
                demandedSlugAtlases.insert(payload);
            else
#endif
                demandedTextures.insert(payload);
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

#ifdef ROCKY_HAS_SLUGHORN
    reg.view<SlugSlotDetail>().each([&](auto entity, auto& detail)
        {
            if (demandedSlugAtlases.find(entity) == demandedSlugAtlases.end())
                releaseSlugSlot(detail);
        });

    std::size_t rejectedSlugCount = 0u;
    for (auto entity : demandedSlugAtlases)
    {
        if (!reg.valid(entity))
            continue;

        const auto* resource = reg.try_get<SlugResource>(entity);
        auto* existing = reg.try_get<SlugSlotDetail>(entity);
        if (!resource || !resource->ready ||
            !resource->curveTexture || !resource->bandTexture)
        {
            if (existing)
                releaseSlugSlot(*existing);
            continue;
        }

        auto& detail = reg.get_or_emplace<SlugSlotDetail>(entity);
        const bool matches =
            detail.descriptorImageIndex >= 0 &&
            detail.descriptorImageIndex < static_cast<std::int32_t>(slugCurves->imageInfoList.size()) &&
            detail.curveTexture == resource->curveTexture &&
            detail.bandTexture == resource->bandTexture &&
            detail.resourceRevision == resource->revision &&
            slugCurves->imageInfoList[detail.descriptorImageIndex] == resource->curveTexture &&
            slugBands->imageInfoList[detail.descriptorImageIndex] == resource->bandTexture;

        if (matches)
        {
            // TerrainNode rebuilds the global descriptor set before ECS systems
            // run. One later frame therefore proves this slot's image pair is
            // installed before its shape metadata is allowed into the SSBO.
            if (!detail.readyForDraw && frame > detail.descriptorWriteFrame)
                detail.readyForDraw = true;
            continue;
        }

        int slot = detail.descriptorImageIndex;
        if (slot < 0)
        {
            for (std::size_t i = 0; i < slugCurves->imageInfoList.size(); ++i)
            {
                if (slugCurves->imageInfoList[i] == _fallbackSlugCurve &&
                    slugBands->imageInfoList[i] == _fallbackSlugBand)
                {
                    slot = static_cast<int>(i);
                    break;
                }
            }
        }

        if (slot < 0)
        {
            ++rejectedSlugCount;
            releaseSlugSlot(detail);
            continue;
        }

        slugCurves->imageInfoList[slot] = resource->curveTexture;
        slugBands->imageInfoList[slot] = resource->bandTexture;
        detail.curveTexture = resource->curveTexture;
        detail.bandTexture = resource->bandTexture;
        detail.descriptorImageIndex = slot;
        detail.resourceRevision = resource->revision;
        detail.descriptorWriteFrame = frame;
        detail.readyForDraw = false;
        requestCompile(resource->curveTexture);
        requestCompile(resource->bandTexture);
        sharedDescriptorsDirty = true;
    }
#endif

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

#ifdef ROCKY_HAS_SLUGHORN
    if (rejectedSlugCount > 0u)
    {
        if (!_slugSlotsExhausted)
        {
            Log()->warn(
                "DecalSystemNode: {} visible Slug atlases exceed the {} available descriptor slots; some will not render",
                demandedSlugAtlases.size(), slugCurves->imageInfoList.size());
        }
        _slugSlotsExhausted = true;
    }
    else
    {
        _slugSlotsExhausted = false;
    }
#endif

    if (sharedDescriptorsDirty)
    {
        requestCompile(textures);
#ifdef ROCKY_HAS_SLUGHORN
        requestCompile(slugCurves);
        requestCompile(slugBands);
#endif
        vsgcontext->sharedRenderData->dirtySharedDescriptors();
        vsgcontext->requestFrame();
    }

    // Drain facade dirty queues; normalized contracts are polled by identity
    // and revision and therefore support multiple independent consumers.
    DecalStyle::eachDirty(reg, [](entt::entity) {});
    Overlay::eachDirty(reg, [](entt::entity) {});
    Decal::eachDirty(reg, [](entt::entity) {});
    ProjectedTexture::eachDirty(reg, [](entt::entity) {});
}

void
DecalSystemNode::resizeGPUBuffersIfNeeded(VSGContext vsgcontext)
{
    bool anyBuffersChanged = false;
    unsigned totalNumDecals = 0u;
#ifdef ROCKY_HAS_SLUGHORN
    unsigned totalNumSlugLayers = 0u;
#endif

    // Recount from registry to keep capacity decisions in sync with actual entities.
    _registry.read([&](entt::registry& reg)
    {
        auto projections = reg.view<ProjectedTexture, ActiveState, Visibility>();
        projections.each([&](auto entity, auto& projected, auto&, auto&)
        {
            const auto payload = projected.texture != entt::null ? projected.texture : entity;
#ifdef ROCKY_HAS_SLUGHORN
            const auto* overlay = reg.try_get<Overlay>(payload);
            if (overlay && resolveOverlayMode(reg, payload, overlay->mode) == OverlayMode::Vector)
            {
                const auto* resource = reg.try_get<SlugResource>(payload);
                const auto* detail = reg.try_get<SlugSlotDetail>(payload);
                if (resource && resource->ready && detail && detail->readyForDraw &&
                    detail->resourceRevision == resource->revision &&
                    resource->textureWidthLog2 > 0u &&
                    resource->indirectionSize == SLUG_INDIRECTION_SIZE)
                {
                    ++totalNumDecals;
                    totalNumSlugLayers += static_cast<unsigned>(resource->layers.size());
                }
            }
            else
#endif
            {
                ++totalNumDecals;
            }
        });
    });

    for (auto& vds : _sharedRenderData->viewDependentState)
    {
        // Private offscreen views and removed views leave holes in the application view table.
        if (!vds || !vds->frustumParamsBuf)
            continue;

        bool buffersChanged = false;
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

#ifdef ROCKY_HAS_SLUGHORN
        if (!vds->slugLayersBuf)
            buffersChanged = true;

        BufferAccess<SlugLayerGPU> slugLayers(
            vds->slugLayersBuf,
            BINDING_VDS_SLUG_LAYERS,
            TYPE_VDS_SLUG_LAYERS);
        const auto currentSlugLayerCapacity = slugLayers.capacity();
        if (totalNumSlugLayers > currentSlugLayerCapacity)
        {
            constexpr std::size_t growBy = 16u;
            const auto newSlugLayerCapacity =
                std::max<std::size_t>(totalNumSlugLayers, currentSlugLayerCapacity + growBy);

            Log()->debug(
                "DecalSystemNode: resizing Slug layer buffer to {} at {} bytes",
                newSlugLayerCapacity,
                newSlugLayerCapacity * sizeof(SlugLayerGPU));

            slugLayers.resize(newSlugLayerCapacity, vsgcontext);
            buffersChanged = true;
        }
#endif

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
            anyBuffersChanged = true;
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

    if (anyBuffersChanged)
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
        if (!view.picks)
            view.picks = std::make_shared<VectorOverlayPickView>();
        view.picks->records.clear();

        if (view.commands && vds)
        {
            auto vm = to_glm(vds->view->camera->viewMatrix->transform());
            BufferAccess<FrustumGridParamsGPU> grid(vds->frustumParamsBuf);
            view.picks->grid = *grid.operator->();
            view.picks->viewport = vds->view->camera->getViewport();
            view.picks->viewMatrix = vm;
            view.picks->projectionMatrix = to_glm(vds->view->camera->projectionMatrix->transform());

            BufferAccess<DecalGPU> gpudecal(vds->decalsBuf);
            const auto gpuCapacity = gpudecal.capacity();
            if (gpuCapacity == 0u)
                continue;

#ifdef ROCKY_HAS_SLUGHORN
            BufferAccess<SlugLayerGPU> gpuSlugLayer(vds->slugLayersBuf);
            const auto slugLayerCapacity = gpuSlugLayer.capacity();
            if (slugLayerCapacity == 0u)
                continue;
#endif

            std::uint32_t decalIndex = 0u;

            // TODO: why are we reading viewport 0 only?
            auto& vp = vds->viewportData->at(0);
            RenderingState rs{
                viewID, (FrameCountType)~0,
                { vp[0], vp[1], vp[2], vp[3] }
            };

            std::vector<DecalGPU> decalRecords;
#ifdef ROCKY_HAS_SLUGHORN
            // A Slug payload still creates one DecalGPU projection. Its atlas
            // shapes are flattened here into a second buffer, with outlines
            // first, and the logical decal stores their range. This keeps
            // culling/order capacity proportional to overlays rather than to
            // the number of shapes inside each overlay.
            std::vector<SlugLayerGPU> slugLayerRecords;
#endif

            _registry.read([&](entt::registry& reg)
            {
                auto applyTexture = [&](entt::entity e_texture, const Color& color, DecalGPU& out, std::int32_t& payloadFlags) -> bool
                {
                    out.color = color;
                    out.textureIndex = -1;
                    payloadFlags = 0;

                    if (auto* resource = reg.try_get<TextureResource>(e_texture))
                    {
                        if (!resource->ready)
                            return false;
                        if (resource->texture)
                        {
                            auto* textureDetail = reg.try_get<TextureSlotDetail>(e_texture);
                            if (!textureDetail || !textureDetail->readyForDraw ||
                                textureDetail->descriptorImageIndex < 0 || textureDetail->texture != resource->texture)
                                return false;
                            out.textureIndex = textureDetail->descriptorImageIndex;
                            if (resource->origin == TextureOrigin::UpperLeft)
                                payloadFlags |= DECAL_FLAG_UPPER_LEFT_TEXTURE_ORIGIN;
                            if (resource->alphaMode == TextureAlphaMode::Premultiplied)
                                payloadFlags |= DECAL_FLAG_PREMULTIPLIED_ALPHA;
                        }
                    }

                    return true;
                };

                auto applyProjection = [&](
                    entt::entity e_projection,
                    entt::entity e_projector,
                    const glm::dmat4& projectorWorld,
                    const ProjectedTexture& projected,
                    DecalGPU& out, glm::dmat4& effectiveWorld) -> bool
                {
                    auto* optics = reg.try_get<Optics>(e_projector);
                    auto* projectionDetails = reg.try_get<ProjectionDetail>(e_projection);
                    auto* projectionDetail = projectionDetails ? &projectionDetails->views[viewID] : nullptr;

                    if (!optics)
                    {
                        if (projected.requireOptics)
                            return false;

                        effectiveWorld = projectorWorld;
                        if (projected.placement == ProjectionPlacement::Terrain &&
                            projectionDetail && projectionDetail->focalPointValid)
                        {
                            effectiveWorld[3] = glm::dvec4(projectionDetail->focalPoint, 1.0);
                            // Tighten only the receiving volume. Source fitting and baked/analytic UVs are unchanged.
                            projectionDetail->applyTerrainDepth(effectiveWorld);
                        }

                        out.mvm = glm::fmat4(vm * effectiveWorld);
                        out.distance = 0.0f;
                        out.zMin = 1.0f;
                        out.zMax = 10.0f;
                        out.cullingRadius = 1.0f;
                        out.tanHalfFovY = 0.0f;
                        out.aspect = 1.0f;
                        return true;
                    }

                    // Optics always project relative to the Transform on the Optics
                    // entity. This also makes an explicitly referenced Optics entity
                    // behave consistently for perspective and orthographic decals.
                    glm::dmat4 opticsModel = projectorWorld * optics->pose;

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

                        const double nearDistance = projectionDetail ?
                            projectionDetail->nearDistance :
                            optics->focalDistance * optics->nearScale + optics->nearBias;
                        const double farDistance = projectionDetail ?
                            projectionDetail->farDistance :
                            optics->focalDistance * optics->farScale + optics->farBias;
                        float tanH = tanf(glm::radians((float)optics->fovY * 0.5f));
                        float nearClip = std::max(1.0f, (float)nearDistance);
                        float farClip = std::max(nearClip + 1.0f, (float)farDistance);

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
                        // scale. Terrain placement only replaces its world-space center.
                        if (projected.placement == ProjectionPlacement::Terrain &&
                            projectionDetail && projectionDetail->focalPointValid)
                        {
                            opticsModel[3] = glm::dvec4(projectionDetail->focalPoint, 1.0);
                        }
                        out.mvm = glm::fmat4(vm * opticsModel);
                        // The inverse is computed once on the CPU below.
                        out.distance = 0.0f; // zero means orthographic
                    }

                    effectiveWorld = opticsModel;
                    return true;
                };

                auto updateProjected = [&](auto entity, auto& projected, auto&, auto& visibility)
                {
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
                    DecalGPU pending{};
                    glm::dmat4 effectiveWorld(1.0);

                    if (!applyProjection(entity, e_projector, modelWorld, projected, pending, effectiveWorld))
                        return;

                    if (pending.distance == 0.0f)
                    {
                        auto* adapter = reg.try_get<DecalFacadeAdapter>(entity);
                        auto* style = reg.try_get<DecalStyle>(e_texture);
                        if (adapter && adapter->ownsProjectedTexture && style && style->textureSize)
                        {
                            const auto dimensions = glm::abs(*style->textureSize);
                            const float xLength = glm::length(glm::fvec3(pending.mvm[0]));
                            const float yLength = glm::length(glm::fvec3(pending.mvm[1]));
                            if (dimensions.x > 0.0 && xLength > 0.0f)
                            {
                                const float scale = static_cast<float>(dimensions.x) / xLength;
                                pending.mvm[0] *= scale;
                                effectiveWorld[0] *= double(scale);
                            }
                            if (dimensions.y > 0.0 && yLength > 0.0f)
                            {
                                const float scale = static_cast<float>(dimensions.y) / yLength;
                                pending.mvm[1] *= scale;
                                effectiveWorld[1] *= double(scale);
                            }
                        }
                    }

                    pending.mvmInverse = glm::inverse(pending.mvm);

#ifdef ROCKY_HAS_SLUGHORN
                    const auto* overlay = reg.try_get<Overlay>(e_texture);
                    if (overlay && resolveOverlayMode(reg, e_texture, overlay->mode) == OverlayMode::Vector)
                    {
                        // Perspective UV derivatives are intentionally deferred from
                        // this first Slug slice; the shader also guards this case.
                        if (pending.distance > 0.0f)
                            return;

                        const auto* resource = reg.try_get<SlugResource>(e_texture);
                        const auto* slugDetail = reg.try_get<SlugSlotDetail>(e_texture);
                        if (!resource || !resource->ready || !slugDetail ||
                            !slugDetail->readyForDraw ||
                            slugDetail->resourceRevision != resource->revision ||
                            slugDetail->descriptorImageIndex < 0 ||
                            resource->textureWidthLog2 == 0u ||
                            resource->indirectionSize != SLUG_INDIRECTION_SIZE)
                            return;

                        pending.textureIndex = slugDetail->descriptorImageIndex;
                        pending.color = projected.color;
                        pending.payloadFlags =
                            DECAL_FLAG_SLUG |
                            ((static_cast<std::int32_t>(resource->textureWidthLog2) &
                                DECAL_SLUG_TEXTURE_WIDTH_LOG2_MASK) <<
                                DECAL_SLUG_TEXTURE_WIDTH_LOG2_SHIFT);

                        // Reorder only the per-view metadata, not the atlas.
                        // The shader uses outlineCount to run a global casing
                        // pass followed by the ordinary/core pass.
                        const auto firstLayer =
                            static_cast<std::uint32_t>(slugLayerRecords.size());
                        const auto* highlight = reg.try_get<Highlight>(entity);
                        auto appendLayer = [&](const SlugLayerResource& layer)
                        {
                            SlugLayerGPU layerRecord;
                            layerRecord.color = layer.color;
                            if (highlight)
                            {
                                const float strength = std::clamp(highlight->color.a, 0.0f, 1.0f);
                                for (int i = 0; i < 3; ++i)
                                    layerRecord.color[i] =
                                        layer.color[i] * (1.0f - strength) + highlight->color[i] * strength;
                            }
                            layerRecord.uvToEmX = layer.uvToEmX;
                            layerRecord.uvToEmY = layer.uvToEmY;
                            layerRecord.bandTransform = layer.bandTransform;
                            layerRecord.shapeData = layer.shapeData;
                            slugLayerRecords.emplace_back(std::move(layerRecord));
                        };

                        for (const auto& layer : resource->layers)
                            if (layer.isOutline)
                                appendLayer(layer);

                        const auto outlineCount =
                            static_cast<std::uint32_t>(slugLayerRecords.size()) - firstLayer;

                        for (const auto& layer : resource->layers)
                            if (!layer.isOutline)
                                appendLayer(layer);

                        const auto layerCount =
                            static_cast<std::uint32_t>(slugLayerRecords.size()) - firstLayer;
                        if (layerCount == 0u)
                            return;

                        pending.slugLayerRange = {
                            firstLayer, outlineCount, layerCount, 0u };
                        view.picks->records.push_back({ pending, glm::inverse(effectiveWorld),
                            entity, e_texture, resource->revision });
                        decalRecords.emplace_back(std::move(pending));
                        return;
                    }
#endif

                    std::int32_t texturePayloadFlags = 0;
                    if (!applyTexture(e_texture, projected.color, pending, texturePayloadFlags))
                        return;
                    pending.payloadFlags = texturePayloadFlags;
                    view.picks->records.push_back({ pending });
                    decalRecords.emplace_back(std::move(pending));
                };

                reg.view<ProjectedTexture, ActiveState, Visibility>().each(
                    [&](auto entity, auto& projected, auto& active, auto& visibility)
                    {
                        updateProjected(entity, projected, active, visibility);
                    });
            });

            auto writePending = [&](const DecalGPU& record)
            {
                ROCKY_HARD_ASSERT(
                    decalIndex + 1u < gpuCapacity,
                    "DecalSystemNode: decal SSBO overflow");

                // Record zero stores the count, so advance before writing.
                ++gpudecal;
                ++decalIndex;
                *gpudecal.operator->() = record;
            };

            for (const auto& record : decalRecords)
                writePending(record);

#ifdef ROCKY_HAS_SLUGHORN
            ROCKY_HARD_ASSERT(
                slugLayerRecords.size() <= slugLayerCapacity,
                "DecalSystemNode: Slug layer SSBO overflow");
            gpuSlugLayer.reset();
            for (const auto& record : slugLayerRecords)
            {
                *gpuSlugLayer.operator->() = record;
                ++gpuSlugLayer;
            }
            gpuSlugLayer.dirty();
#endif

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
    static_cast<DecalDebugNode*>(_debugNode.get())->initialize(vsgcontext);

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
    auto* debug = static_cast<DecalDebugNode*>(_debugNode.get());
    debug->enabled = debugVolumes && status.ok();
    debug->seeThrough = debugVolumesSeeThrough;
    if (status.failed())
        return;

    _registry.write([&](entt::registry& r)
        {
            // Synchronize facade-owned projections every frame. This
            // deliberately supports the established pattern of editing public
            // component fields by reference instead of registry.patch().
            r.view<Decal, DecalFacadeAdapter, ProjectedTexture>().each(
                [&](auto entity, auto& decal, auto& adapter, auto& projected)
                {
                    if (!adapter.ownsProjectedTexture)
                        return;
                    projected.texture = decal.style != entt::null ? decal.style : entity;
                    projected.projector = decal.optics != entt::null ? decal.optics : entity;
                    projected.placement = decal.placement;
                    projected.computeClipRange = decal.computeClipRange;
                    projected.requireOptics = decal.optics != entt::null;
                    projected.color = StockColor::White;
                    if (auto* style = r.try_get<DecalStyle>(projected.texture))
                        projected.color = style->color;
                });

            r.view<Overlay, OverlayProjectionFacadeAdapter, ProjectedTexture>().each(
                [&](auto entity, auto& overlay, auto& adapter, auto& projected)
                {
                    if (!adapter.ownsProjectedTexture)
                        return;
                    projected.texture = entity;
                    projected.projector = entity;
                    projected.color = overlay.color;
                    projected.placement = ProjectionPlacement::Terrain;
                    projected.computeClipRange = true;
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
        // View IDs are not contiguous: an empty slot must not hide later application views.
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

#ifdef ROCKY_HAS_SLUGHORN
namespace ROCKY_NAMESPACE::detail
{
    //! Samples only resident vector candidates, in reverse compositing order, under the registry read lock.
    //! The receiver and screen differentials share the submitted projection's world coordinate system.
    static entt::entity sampleVectorCandidates(entt::registry& reg,
        const std::array<const VectorOverlayPickRecord*, MAX_DECALS_PER_TILE>& candidates, unsigned count,
        const glm::dvec3& world, const glm::dvec3& dx, const glm::dvec3& dy,
        const RenderingState& rs, float minAlpha)
    {
        // Reverse both global passes and the submitted layer order. A core from any instance is
        // above every outline; alpha is a contribution threshold, not a scene visibility test.
        for (int pass = 1; pass >= 0; --pass)
        {
            for (unsigned index = count; index > 0u; --index)
            {
                const auto& record = *candidates[index - 1u];
                if (!reg.valid(record.entity) || !reg.valid(record.payload) ||
                    !reg.all_of<ActiveState, Visibility>(record.entity))
                    continue;
                const auto* projected = reg.try_get<ProjectedTexture>(record.entity);
                const auto* overlay = reg.try_get<Overlay>(record.payload);
                const auto* resource = reg.try_get<SlugResource>(record.payload);
                const auto* slot = reg.try_get<SlugSlotDetail>(record.payload);
                if (!projected || (projected->texture == entt::null ? record.entity : projected->texture) != record.payload ||
                    !overlay || resolveOverlayMode(reg, record.payload, overlay->mode) != OverlayMode::Vector ||
                    !resource || !resource->ready || resource->revision != record.revision || !slot ||
                    !slot->readyForDraw || slot->descriptorImageIndex < 0 || slot->resourceRevision != record.revision)
                    continue;
                if (!visible(reg.get<Visibility>(record.entity), rs))
                    continue;
                const glm::dvec3 local = record.worldToLocal * glm::dvec4(world, 1.0);
                if (!std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z) ||
                    glm::any(glm::greaterThan(glm::abs(local), glm::dvec3(0.5))))
                    continue;
                const glm::dvec2 uv = glm::dvec2(local) + 0.5;
                const glm::dvec2 localDx = record.worldToLocal * glm::dvec4(dx, 0.0);
                const glm::dvec2 localDy = record.worldToLocal * glm::dvec4(dy, 0.0);
                for (auto layer = resource->layers.rbegin(); layer != resource->layers.rend(); ++layer)
                {
                    const float alpha = layer->color.a * record.decal.color.a;
                    if (layer->isOutline != (pass == 0) || !layer->coverage || !std::isfinite(alpha) || alpha < minAlpha)
                        continue;
                    const glm::dvec2 rowX(layer->uvToEmX);
                    const glm::dvec2 rowY(layer->uvToEmY);
                    const double emX = glm::dot(uv, rowX) + layer->uvToEmX.z;
                    const double emY = glm::dot(uv, rowY) + layer->uvToEmY.z;
                    const double width = std::abs(glm::dot(localDx, rowX)) + std::abs(glm::dot(localDy, rowX));
                    const double height = std::abs(glm::dot(localDx, rowY)) + std::abs(glm::dot(localDy, rowY));
                    if (layer->coverage->sample(float(emX), float(emY), float(width), float(height)) * alpha >= minAlpha)
                    {
                        return record.entity;
                    }
                }
            }
        }
        return entt::null;
    }
}
#endif

// The SDK remains behind SlugCoverage; all world/projector and pixel-ray math here is C++17 and double precision.
entt::entity
DecalSystemNode::intersectVectorOverlay(View& view, int x, int y, float minAlpha)
{
#ifdef ROCKY_HAS_SLUGHORN
    if (!view || !view.vsgView->camera || status.failed() ||
        !std::isfinite(minAlpha) || minAlpha <= 0.0f || minAlpha > 1.0f)
        return entt::null;
    const auto viewID = view.vsgView->viewID;
    if (viewID >= _views.size())
        return entt::null;
    const auto& picks = _views[viewID].picks;
    if (!picks || picks->records.empty())
        return entt::null;
    auto& camera = *view.vsgView->camera;
    const auto vp = camera.getViewport();
    const auto& submittedViewport = picks->viewport;
    if (vp.width <= 0.0f || vp.height <= 0.0f ||
        vp.x != submittedViewport.x || vp.y != submittedViewport.y ||
        vp.width != submittedViewport.width || vp.height != submittedViewport.height ||
        vp.minDepth != submittedViewport.minDepth || vp.maxDepth != submittedViewport.maxDepth)
        return entt::null;
    // Camera changes need a new submission before its cell lists can be queried consistently.
    if (to_glm(camera.viewMatrix->transform()) != picks->viewMatrix ||
        to_glm(camera.projectionMatrix->transform()) != picks->projectionMatrix)
        return entt::null;
    glm::vec4 bounds;
    if (!pickCellBounds(picks->grid, x, y, bounds))
        return entt::null;

    std::array<const VectorOverlayPickRecord*, MAX_DECALS_PER_TILE> candidates{};
    unsigned count = 0u;
    for (const auto& record : picks->records)
    {
        if (pickCellIntersects(record.decal, bounds, picks->grid.projIsOrtho != 0u))
        {
            candidates[count++] = &record;
            if (count == candidates.size())
                break;
        }
    }
    if (count == 0u)
        return entt::null;
    auto hit = geoPointAtWindowCoords(view, x, y);
    if (!hit)
        return entt::null;
    const glm::dvec3 world = hit->point;
    const glm::dvec3 normal = hit->normal;
    const auto inverseVP = glm::inverse(picks->projectionMatrix * picks->viewMatrix);
    glm::dvec3 center, right, down;
    if (!pickPlanePoint(inverseVP, vp, x, y, world, normal, center) ||
        !pickPlanePoint(inverseVP, vp, x + 1.0, y, world, normal, right) ||
        !pickPlanePoint(inverseVP, vp, x, y + 1.0, world, normal, down))
        return entt::null;
    const glm::dvec3 dx = right - center;
    const glm::dvec3 dy = down - center;
    entt::entity result = entt::null;
    _registry.read([&](entt::registry& reg)
    {
        const RenderingState rs{ viewID, (FrameCountType)~0,
            { float(vp.x), float(vp.y), float(vp.width), float(vp.height) } };
        result = sampleVectorCandidates(reg, candidates, count, world, dx, dy, rs, minAlpha);
    });
    return result;
#else
    return entt::null;
#endif
}
