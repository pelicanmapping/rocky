/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

#include "ViewDependentState.h"
#include "ShaderDefines.h"
#include "MapNode.h"
#include "ecs/OverlayRenderContext.h"

using namespace ROCKY_NAMESPACE;


ViewDependentStateEx::ViewDependentStateEx(vsg::ref_ptr<vsg::View> vsgView, vsg::ref_ptr<vsg::Device> device) :
    Inherit(vsgView), _device(device)
{
    //nop
}

void
ViewDependentStateEx::init(vsg::ResourceRequirements& req)
{
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
    renderParams->inverseViewMatrix = to_glm(view->camera->viewMatrix->inverse());
    renderParams->renderDomain = (renderDomain == detail::RenderDomain::OverlayBake) ? 1.0f : 0.0f;

    // ellipsoid params (TODO: don't need to update these constantly!)
    if (!_mapNode)
        _mapNode = detail::find<MapNode>(view);

    if (auto mapNode = _mapNode.ref_ptr())
    {
        renderParams->ellipsoidAxes.x = mapNode->srs().ellipsoid().semiMajorAxis();
        renderParams->ellipsoidAxes.y = mapNode->srs().ellipsoid().semiMinorAxis();
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
