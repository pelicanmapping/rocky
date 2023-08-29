/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky_vsg/Mesh.h>
#include <rocky_vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;

    /**
     * Creates commands for rendering mesh primitives and holds the singleton pipeline
     * configurator for their drawing state.
     */
    class ROCKY_VSG_EXPORT MeshSystem : public vsg::Inherit<ECS::SystemNode, MeshSystem>
    {
    public:
        //! Construct the mesh renderer
        MeshSystem(entt::registry& registry);

        enum Features
        {
            NONE = 0x0,
            TEXTURE = 0x1,
            DYNAMIC_STYLE = 0x2,
            WRITE_DEPTH = 0x4,
            NUM_PIPELINES = 8
        };

        static int featureMask(const Mesh& mesh);

        void initialize(Runtime&) override;

        ROCKY_VSG_SYSTEM_HELPER(Mesh, helper);
    };

    class SelfContainedNodeSystem : public vsg::Inherit<ECS::SystemNode, SelfContainedNodeSystem>
    {
    public:
        SelfContainedNodeSystem(entt::registry& registry) :
            vsg::Inherit<ECS::SystemNode, SelfContainedNodeSystem>(registry),
            helper(registry) { }

        ROCKY_VSG_SYSTEM_HELPER(ECS::NodeComponent, helper);
    };
}

