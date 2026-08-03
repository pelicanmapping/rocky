/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Common.h>
#include <rocky/vsg/ShaderDefines.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    class MapNode;

    struct RenderParamsGPU {
        glm::fmat4 viewMatrix;
        glm::fmat4 inverseViewMatrix;
        glm::fvec2 ellipsoidAxes;
        glm::uint32_t stereographic; // bool
        glm::float32_t _padding[1];
    };
    //static_assert(sizeof(RenderParams) % 16 == 0, "RenderParams must be 16-byte aligned");

    struct FrustumGridParamsGPU {
        glm::fmat4 invProjMatrix;
        glm::ivec4 viewport = { 0, 0, 0, 0 };
        glm::uvec2 numTiles = { 1u, 1u };
        glm::uint32_t pixelsPerTile = 16u;
        glm::uint32_t projIsOrtho = 0;
        glm::float32_t debugTiles = 0.0f;
    };
    //static_assert(sizeof(FrustumGridParams) % 16 == 0, "FrustumGridParams must be 16-byte aligned");

    struct FrustumGPU {
        glm::fvec4 planes[4];
    };
    static_assert(sizeof(FrustumGPU) % 16 == 0, "FrustumGPU must be 16-byte aligned");

    struct DecalTileGPU {
        glm::uint32_t count = 0;
        glm::uint32_t indices[MAX_DECALS_PER_TILE];
    };
    static_assert(sizeof(DecalTileGPU) % 16 == 0, "DecalTileGPU must be 16-byte aligned");

    /**
    * Extends vsg::ViewDependentState to add data for Rocky rendering.
    *
    * Note! Even though we replace each vsg::View's VDS with this one, VSG will still make its OWN
    * View's with stock ViewDependentState objects for shadows (and perhaps other things).
    * So we need to be careful to only use this class when we know it's ours, and not a stock VDS.
    */
    class ROCKY_EXPORT ViewDependentStateEx : public vsg::Inherit<vsg::ViewDependentState, ViewDependentStateEx>
    {
    public:
        ViewDependentStateEx(vsg::ref_ptr<vsg::View> vsgView, vsg::ref_ptr<vsg::Device> device);

        mutable vsg::ref_ptr<vsg::DescriptorBuffer> renderParamsBuf;
        vsg::ref_ptr<vsg::DescriptorBuffer> frustumParamsBuf;
        vsg::ref_ptr<vsg::DescriptorBuffer> frustumsBuf;
        vsg::ref_ptr<vsg::DescriptorBuffer> decalTilesBuf;

        Revision revision = 0;

        vsg::ref_ptr<vsg::Device> device() { return _device; }

    public:
        void init(vsg::ResourceRequirements& req) override;

        void traverse(vsg::RecordTraversal& rt) const override;

    protected:
        mutable vsg::observer_ptr<MapNode> _mapNode;
        vsg::ref_ptr<vsg::Device> _device;
    };
    

    //! Convenience function that adds the VDS descriptor bindings to a shader set.
    extern ROCKY_EXPORT void addViewDependentStateToShaderSet(
        vsg::ShaderSet* shaderSet,
        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL);


    //! Convenience function that enables VDS uniforms on a pipeline.
    extern ROCKY_EXPORT void enableViewDependentStateUniforms(
        vsg::GraphicsPipelineConfigurator* gpc);


    //! Retrieve the VDS from a view.
    inline ViewDependentStateEx* viewDependentState(vsg::View* view)
    {
        if (view)
            return dynamic_cast<ViewDependentStateEx*>(view->viewDependentState.get());
        else
            return nullptr;
    }
}
