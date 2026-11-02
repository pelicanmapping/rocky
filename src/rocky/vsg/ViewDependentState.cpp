/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

#include "ViewDependentState.h"
#include "ShaderDefines.h"
#include "MapNode.h"
#include "ecs/OverlayRenderContext.h"
#include <cmath>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Returns the ellipsoid's surface-horizon plane in view space, keeping its eye-facing half-space.
    //! Uses double precision before uploading floats; invalid inputs or an eye inside the ellipsoid disable clipping.
    glm::vec4 surfaceHorizonPlane(const Ellipsoid& ellipsoid, const glm::dmat4& inverseView, bool orthographic)
    {
        const glm::vec4 disabled(0.0f, 0.0f, 0.0f, 1.0f);
        const glm::dvec3 axes(ellipsoid.semiMajorAxis(), ellipsoid.semiMajorAxis(), ellipsoid.semiMinorAxis());
        if (!(axes.x > 0.0 && axes.z > 0.0))
            return disabled;

        const glm::dvec3 eye(inverseView[3]);
        const auto scaledEye = eye / axes;
        const double eyeDistance2 = glm::dot(scaledEye, scaledEye);
        if (!std::isfinite(eyeDistance2) || eyeDistance2 <= 1.0)
            return disabled;

        // Perspective: (eye / axes^2) dot point = 1 is the plane of ellipsoid tangency.
        // Orthographic: use the direction toward the viewer, including off-center cameras.
        const glm::dvec3 towardEye = orthographic ? glm::dvec3(inverseView[2]) : eye;
        const auto normal = towardEye / (axes * axes);
        const double normalLength = glm::length(normal);
        if (!std::isfinite(normalLength) || normalLength <= 0.0)
            return disabled;

        glm::dvec4 plane(normal / normalLength, orthographic ? 0.0 : -1.0 / normalLength);
        // Retain a small world-space margin for surface tessellation and float precision at the limb.
        plane.w += 10.0; // meters
        plane = glm::transpose(inverseView) * plane;
        for (int i = 0; i < 4; ++i)
            if (!std::isfinite(plane[i]))
                return disabled;
        return glm::vec4(plane);
    }
}


ViewDependentStateEx::ViewDependentStateEx(vsg::ref_ptr<vsg::View> vsgView, vsg::ref_ptr<vsg::Device> device) :
    Inherit(vsgView), _device(device)
{
    //nop
}

void
ViewDependentStateEx::init(vsg::ResourceRequirements& req)
{
    // VSG can collect requirements again when a view is compiled dynamically.
    // Match the base class's one-time initialization without duplicating Rocky bindings.
    if (renderParamsBuf)
        return;

    Inherit::init(req);

    BufferAccess<RenderParamsGPU> renderParams(
        renderParamsBuf,
        BINDING_VDS_RENDER_PARAMS, TYPE_VDS_RENDER_PARAMS);

    renderParams.data()->properties.dataVariance = vsg::DYNAMIC_DATA;

    BufferAccess<FrustumGridParamsGPU> frustumParams(
        frustumParamsBuf,
        BINDING_VDS_FRUSTUM_GRID_PARAMS, TYPE_VDS_FRUSTUM_GRID_PARAMS);

    GPUOnlyBufferAccess<FrustumGPU> buf(frustumsBuf,
        BINDING_VDS_FRUSTUMS, TYPE_VDS_FRUSTUMS,
        _device);

    // add it! It will automatically compile along with the others.
    //this->descriptorSet->descriptors.emplace_back(_myDescriptors.ubo);
    this->descriptorSet->descriptors.emplace_back(renderParamsBuf);
    this->descriptorSet->descriptors.emplace_back(frustumParamsBuf);
    this->descriptorSet->descriptors.emplace_back(frustumsBuf);

    // add its shader-stage binding to the layout.
    this->descriptorSetLayout->addBinding(BINDING_VDS_RENDER_PARAMS,
        TYPE_VDS_RENDER_PARAMS, 1, VK_SHADER_STAGE_ALL);

    this->descriptorSetLayout->addBinding(BINDING_VDS_FRUSTUM_GRID_PARAMS,
        TYPE_VDS_FRUSTUM_GRID_PARAMS, 1, VK_SHADER_STAGE_ALL);

    this->descriptorSetLayout->addBinding(BINDING_VDS_FRUSTUMS,
        TYPE_VDS_FRUSTUMS, 1, VK_SHADER_STAGE_ALL);


#ifdef ROCKY_HAS_DECALS
    BufferAccess<DecalGPU> decals(decalsBuf,
        BINDING_VDS_DECALS, TYPE_VDS_DECALS);

    // A newly added view can render before the next DecalSystem update/compute pass.
    // Its header must describe an empty list, not DecalGPU's default texture index (-1).
    decals->count = 0;

    this->descriptorSet->descriptors.emplace_back(decalsBuf);

    this->descriptorSetLayout->addBinding(BINDING_VDS_DECALS,
        TYPE_VDS_DECALS, 1, VK_SHADER_STAGE_ALL);


    GPUOnlyBufferAccess<DecalTileGPU> decalTiles(decalTilesBuf,
        BINDING_VDS_DECAL_TILES, TYPE_VDS_DECAL_TILES,
        _device);

    this->descriptorSet->descriptors.emplace_back(decalTilesBuf);

    this->descriptorSetLayout->addBinding(BINDING_VDS_DECAL_TILES,
        TYPE_VDS_DECAL_TILES, 1, VK_SHADER_STAGE_ALL);

#ifdef ROCKY_HAS_SLUGHORN
    BufferAccess<SlugLayerGPU> slugLayers(slugLayersBuf,
        BINDING_VDS_SLUG_LAYERS, TYPE_VDS_SLUG_LAYERS);

    this->descriptorSet->descriptors.emplace_back(slugLayersBuf);

    this->descriptorSetLayout->addBinding(BINDING_VDS_SLUG_LAYERS,
        TYPE_VDS_SLUG_LAYERS, 1, VK_SHADER_STAGE_ALL);
#endif
#endif // ROCKY_HAS_DECALS
}

void
ViewDependentStateEx::traverse(vsg::RecordTraversal& rt) const
{
    // todo: update custom descriptors
    BufferAccess<RenderParamsGPU> renderParams(renderParamsBuf);

    auto [renderDomain, overlayTarget] = detail::getRenderDomainAndOverlayTarget(rt);
    (void)overlayTarget;

    renderParams->viewMatrix = to_glm(view->camera->viewMatrix->transform());
    const auto inverseView = to_glm(view->camera->viewMatrix->inverse());
    renderParams->inverseViewMatrix = inverseView;
    // Reset every traversal so switching maps or reusing a view for baking cannot retain a stale plane.
    renderParams->horizonPlane = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    renderParams->renderDomain = (renderDomain == detail::RenderDomain::OverlayBake) ? 1.0f : 0.0f;

    // ellipsoid params (TODO: don't need to update these constantly!)
    if (!_mapNode)
        _mapNode = detail::find<MapNode>(view);

    if (auto mapNode = _mapNode.ref_ptr())
    {
        renderParams->ellipsoidAxes.x = mapNode->srs().ellipsoid().semiMajorAxis();
        renderParams->ellipsoidAxes.y = mapNode->srs().ellipsoid().semiMinorAxis();
        if (mapNode->srs().isGeocentric() && renderDomain != detail::RenderDomain::OverlayBake)
        {
            const bool orthographic = view->camera->projectionMatrix->transform()(3, 3) != 0.0;
            renderParams->horizonPlane = surfaceHorizonPlane(mapNode->srs().ellipsoid(), inverseView, orthographic);
        }
    }

    renderParams.dirty();

    Inherit::traverse(rt);
}



void
ROCKY_NAMESPACE::addViewDependentStateToShaderSet(vsg::ShaderSet* shaderSet, VkShaderStageFlags stageFlags)
{
    // VSG view-dependent data. You must include it all even if you only intend to use
    // one of the uniforms.
    shaderSet->customDescriptorSetBindings.push_back(
        vsg::ViewDependentStateBinding::create(DESCRIPTOR_SET_VDS));

    shaderSet->addDescriptorBinding(
        "vsg_lights", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_VSG_LIGHTS,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        stageFlags, {});

    // VSG viewport state
    shaderSet->addDescriptorBinding(
        "vsg_viewports", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_VSG_VIEWPORTS,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        stageFlags, {});

    shaderSet->addDescriptorBinding(
        "rockyvds_render_params", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_RENDER_PARAMS,
        TYPE_VDS_RENDER_PARAMS, 1,
        stageFlags, {});

    shaderSet->addDescriptorBinding(
        "rockyvds_frustum_grid_params", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_FRUSTUM_GRID_PARAMS,
        TYPE_VDS_FRUSTUM_GRID_PARAMS, 1,
        stageFlags, {});

    shaderSet->addDescriptorBinding(
        "rockyvds_frustums", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_FRUSTUMS,
        TYPE_VDS_FRUSTUMS, 1,
        stageFlags, {});

#ifdef ROCKY_HAS_DECALS
    shaderSet->addDescriptorBinding(
        "rockyvds_decals", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_DECALS,
        TYPE_VDS_DECALS, 1,
        stageFlags, {});

    shaderSet->addDescriptorBinding(
        "rockyvds_decal_tiles", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_DECAL_TILES,
        TYPE_VDS_DECAL_TILES, 1,
        stageFlags, {});

#ifdef ROCKY_HAS_SLUGHORN
    shaderSet->addDescriptorBinding(
        "rockyvds_slug_layers", "",
        DESCRIPTOR_SET_VDS,
        BINDING_VDS_SLUG_LAYERS,
        TYPE_VDS_SLUG_LAYERS, 1,
        stageFlags, {});
#endif
#endif
}

void
ROCKY_NAMESPACE::enableViewDependentStateUniforms(vsg::GraphicsPipelineConfigurator* gpc)
{
    gpc->enableDescriptor("vsg_lights");
    gpc->enableDescriptor("vsg_viewports");

    gpc->enableDescriptor("rockyvds_render_params");
    gpc->enableDescriptor("rockyvds_frustum_grid_params");
    gpc->enableDescriptor("rockyvds_frustums");

#ifdef ROCKY_HAS_DECALS
    gpc->enableDescriptor("rockyvds_decals");
    gpc->enableDescriptor("rockyvds_decal_tiles");
#ifdef ROCKY_HAS_SLUGHORN
    gpc->enableDescriptor("rockyvds_slug_layers");
#endif
#endif
}
