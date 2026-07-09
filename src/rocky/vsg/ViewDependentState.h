/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
//#include <rocky/vsg/SharedRenderData.h>
#include <rocky/vsg/Common.h>
#include <rocky/Math.h>

namespace ROCKY_NAMESPACE
{
    class MapNode;


    struct RenderParams {
        vsg::mat4 inverseViewMatrix;
        vsg::vec2 ellipsoidAxes;
        std::uint32_t stereographic; // bool
        float _padding[1];
    };
    static_assert(sizeof(RenderParams) % 16 == 0, "RenderParams must be 16-byte aligned");

    struct FrustumGridParams {
        glm::fmat4 invProjMatrix;
        glm::ivec4 viewport = { 0, 0, 0, 0 };
        glm::uvec2 numTiles = { 1u, 1u };
        glm::uint32_t pixelsPerTile = 16u;
        glm::float32_t debugTiles = 0.0f;
    };
    static_assert(sizeof(FrustumGridParams) % 16 == 0, "FrustumGridParams must be 16-byte aligned");

    struct FrustumGPU {
        glm::fvec4 planes[4];
    };

    /**
    * Extends vsg::ViewDependentState to add data for Rocky rendering.
    * Shader usage (where "binding" === BINDING_ROCKY_VIEW_DEPENDENT_STATE)
    *
    *   layout(set = 1, binding = XXX) uniform RockyVDS {
    *       mat4 inverseViewMatrix;
    *       vec2 ellipsoidAxes;
    *       uint stereographic;
    *       float _padding[1];
    *   } u_vds;
    *
    */
    class ROCKY_EXPORT ViewDependentStateEx : public vsg::Inherit<vsg::ViewDependentState, ViewDependentStateEx>
    {
    public:
        ViewDependentStateEx(vsg::ref_ptr<vsg::View> vsgView) : Inherit(vsgView) {
            //nop
        }

        mutable vsg::ref_ptr<vsg::DescriptorBuffer> renderParamsBuf;
        mutable vsg::ref_ptr<vsg::DescriptorBuffer> frustumParamsBuf;
        vsg::ref_ptr<vsg::DescriptorBuffer> frustumsBuf;

#if 0
        struct MyDescriptors
        {
            struct Uniforms
            {
                vsg::mat4 inverseViewMatrix;
                vsg::vec2 ellipsoidAxes;
                std::uint32_t stereographic; // bool
                float _padding[1];
            };
            vsg::ref_ptr<vsg::DescriptorBuffer> ubo;

            vsg::ref_ptr<vsg::DescriptorBuffer> frustumParams;
            vsg::ref_ptr<vsg::DescriptorBuffer> frustums;
        };

        MyDescriptors::Uniforms& uniforms() {
            return *static_cast<MyDescriptors::Uniforms*>(_myDescriptors.ubo->bufferInfoList[0]->data->dataPointer());
        }

        void dirty() {
            _myDescriptors.ubo->bufferInfoList[0]->data->dirty();
        }
#endif

    public:
        void init(vsg::ResourceRequirements& req) override;

        void traverse(vsg::RecordTraversal& rt) const override;

    protected:
        //MyDescriptors _myDescriptors;
        mutable vsg::observer_ptr<MapNode> _mapNode;
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
