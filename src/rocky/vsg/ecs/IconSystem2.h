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
    //! Indirect setup (set = 0)
    //!   binding = 0 : command buffer
    //!   binding = 1 : cull buffer
    //!   binding = 2 : draw buffer
    //!   binding = 3 : sampler uniform
    //!   binding = 4 : array of textures uniform

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

        //! Initialize the system (once)
        void initializeSystem(VSGContext&) override;

        //! Update pass (called once per frame before recording starts)
        void update(VSGContext&) override;

        //! Standard recording traversal.
        void traverse(vsg::RecordTraversal& rt) const override;

    protected:
        virtual ~IconSystem2Node();

    private:

        // cache of image descriptors so we can re-use textures & samplers
        mutable std::unordered_map<std::shared_ptr<Image>, vsg::ref_ptr<vsg::DescriptorImage>> descriptorImage_cache;
        mutable std::mutex mutex;

        vsg::ref_ptr<vsg::Dispatch> cullDispatch;

        vsg::ref_ptr<vsg::ubyteArray> drawCommandBuffer;
        vsg::ref_ptr<vsg::ubyteArray> cullBuffer;

        vsg::ref_ptr<vsg::Sampler> sampler;
        vsg::ImageInfoList textures;

        mutable int dirtyCount = 0;
    };
}
