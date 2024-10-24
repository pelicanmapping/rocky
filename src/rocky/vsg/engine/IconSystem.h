/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Icon.h>
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    /**
     * Creates commands for rendering icon primitives.
     */
    class ROCKY_EXPORT IconSystemNode : public vsg::Inherit<ECS::SystemNode, IconSystemNode>
    {
    public:
        //! Construct the mesh renderer
        IconSystemNode(entt::registry& r);

        //! Features supported by this renderer
        enum Features
        {
            NONE = 0x0,
            NUM_PIPELINES = 1
        };

        //! Get the feature mask for a given icon
        static int featureMask(const Icon& icon);

        //! Initialize the system (once)
        void initializeSystem(Runtime&) override;

        ROCKY_SYSTEMNODE_HELPER(Icon, helper);

    private:

        //! Called by the helper to initialize a new node component.
        void initializeComponent(Icon& icon, InitContext& c);

        // cache of image descriptors so we can re-use textures
        std::unordered_map<std::shared_ptr<Image>, vsg::ref_ptr<vsg::DescriptorImage>> diCache;
    };
}
