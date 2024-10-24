/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/vsg/Mesh.h>
#include <rocky/vsg/ECS.h>

namespace ROCKY_NAMESPACE
{
    class Runtime;
    class MeshSystemNode;

    class ROCKY_EXPORT MeshSystem
    {
    };

    /**
    * VSG node that renders Mesh components.
    */
    class ROCKY_EXPORT MeshSystemNode :
        public vsg::Inherit<ECS::SystemNode, MeshSystemNode>,
        public MeshSystem
    {
    public:
        //! Construct the mesh renderer
        MeshSystemNode(entt::registry& registry);

        //! Supported features in a mask format
        enum Features
        {
            NONE           = 0,
            TEXTURE        = 1 << 0,
            DYNAMIC_STYLE  = 1 << 1,
            WRITE_DEPTH    = 1 << 2,
            CULL_BACKFACES = 1 << 3,
            NUM_PIPELINES  = 16
        };

        //! Returns a mask of supported features for the given mesh
        static int featureMask(const Mesh& mesh);

        //! One time initialization of the system        
        void initializeSystem(Runtime&) override;

        ROCKY_SYSTEMNODE_HELPER(Mesh, helper);

    private:

        //! Called by the helper to initialize a new node component.
        void initializeComponent(Mesh& mesh, InitContext& c);
    };

#if 0
    /**
     * Creates commands for rendering mesh primitives and holds the singleton pipeline
     * configurator for their drawing state.
     */
    class ROCKY_EXPORT MeshSystem : public ECS::VSG_System
    {
    public:
        MeshSystem(entt::registry& registry) :
            ECS::VSG_System(registry) { }

        vsg::ref_ptr<ECS::VSG_SystemNode> getOrCreateNode() override {
            if (!node)
                node = MeshSystemNode::create(registry);
            return node;
        }
    };
#endif

    //TODO-- Move this into its own header.

    class ROCKY_EXPORT NodeSystem
    {
    };

    /**
    * VSG node that renders Node components (just plain vsg nodes)
    */
    class ROCKY_EXPORT NodeSystemNode :
        public vsg::Inherit<ECS::SystemNode, NodeSystemNode>,
        public NodeSystem
    {
    public:
        NodeSystemNode(entt::registry& registry) :
            Inherit<ECS::SystemNode, NodeSystemNode>(registry),
            helper(registry) { }

        ROCKY_SYSTEMNODE_HELPER(ECS::NodeComponent, helper);
    };

#if 0
    /**
    * System for managing vsg Nodes
    */
    class ROCKY_EXPORT NodeSystem : public ECS::VSG_System
    {
    public:
        NodeSystem(entt::registry& registry_) :
            ECS::VSG_System(registry_) { }

        vsg::ref_ptr<ECS::VSG_SystemNode> getOrCreateNode() override {
            if (!node)
                node = NodeSystemNode::create(registry);
            return node;
        }
    };
#endif
}
