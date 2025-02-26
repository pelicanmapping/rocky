/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/ecs/Icon.h>
#include <rocky/vsg/ecs/ECSNode.h>
#include <rocky/vsg/PipelineState.h>

namespace ROCKY_NAMESPACE
{
    //! Instance buffer as mirrored in the culling compute shader
    struct IconInstanceGPU
    {
        vsg::mat4 proj;
        vsg::mat4 modelview;
        vsg::vec4 viewport = { 0,0,0,0 };   // x,y = lower left, z,w = width, height
        float rotation = 0.0f;              // radians
        float size = 0.0f;                  // pixels
        std::int32_t texture_index = 0;     // texture array index

        float padding[1];
        // keep me 16-byte aligned with padding please
    };

    /**
     * Creates commands for rendering icon primitives using indirect rendering
     */
    class ROCKY_EXPORT IconSystem2Node : public vsg::Inherit<vsg::Group, IconSystem2Node>,
        public ecs::System
    {
    public:
        //! Construct the mesh renderer
        IconSystem2Node(ecs::Registry& r);

        //! Initialize the system (called once at startup)
        void initialize(VSGContext&) override;

        //! Update pass (called once per frame before recording starts)
        void update(VSGContext&) override;

    protected:
        virtual ~IconSystem2Node();

    private:

        // cache of image descriptors so we can re-use textures & samplers
        mutable std::unordered_map<std::shared_ptr<Image>, vsg::ref_ptr<vsg::DescriptorImage>> descriptorImage_cache;
        mutable std::mutex mutex;

        // dispatch command for the GPU culler
        vsg::ref_ptr<vsg::Dispatch> cull_dispatch;

        // The VkDrawIndexedIndirect commmand, which th GPU culler will
        // write to and the rendering will read from
        vsg::ref_ptr<StreamingGPUBuffer> indirect_command;

        // The list of InstanceGPU objects that the GPU culler will read from
        vsg::ref_ptr<StreamingGPUBuffer> cull_list;

        // GPU-side draw list binding
        vsg::ref_ptr<vsg::DescriptorBuffer> draw_list_descriptor;

        // array of textures
        vsg::ref_ptr<vsg::Sampler> sampler;
        vsg::ImageInfoList textures;

        mutable int dirtyCount = 0;

        void buildCullStage(VSGContext& context);

        void buildRenderStage(VSGContext& context);
    };
}
